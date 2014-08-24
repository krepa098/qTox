/*
    Copyright (C) 2014 by Project Tox <https://tox.im>

    This file is part of qTox, a Qt-based graphical interface for Tox.

    This program is libre software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the COPYING file for more details.
*/

#include "avmodule.h"

#include <QMutexLocker>
#include <QDebug>
#include <tox/toxav.h>

#define U8Ptr(x) reinterpret_cast<uint8_t*>(x)

/********************
 * ToxCall
 ********************/

ToxCall::ToxCall(int callIndex, int friendnumber, QAudioFormat format)
    : m_audioFormat(format)
    , m_audioOutput(nullptr)
    , m_audioBuffer(nullptr)
    , m_callIndex(callIndex)
    , m_friendnumber(friendnumber)
{
    m_audioOutput = new QAudioOutput(format, this);
    m_audioOutput->setBufferSize(1024*128); // buffer size needs tweaking (latency)
    m_audioBuffer = m_audioOutput->start();
}

void ToxCall::writeAudio(const QByteArray &data)
{
    if (m_audioBuffer)
        m_audioBuffer->write(data);
}

/********************
 * CoreAVModule
 ********************/

CoreAVModule::CoreAVModule(QObject* parent, Tox* tox, QMutex* mutex)
    : CoreModule(parent, tox, mutex)
    , m_toxAV(nullptr)
    , m_audioSource(nullptr)
    , m_audioInputBuffer(nullptr)
{
    m_toxAV = toxav_new(tox, TOXAV_MAXCALLS);

    // audio & video
    toxav_register_audio_recv_callback(m_toxAV, callbackAudioRecv, this);
    toxav_register_video_recv_callback(m_toxAV, callbackVideoRecv, this);

    // callstates
    // requests
    toxav_register_callstate_callback(m_toxAV, callbackAvInvite, av_OnInvite, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvStart, av_OnStart, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvCancel, av_OnCancel, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvReject, av_OnReject, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvEnd, av_OnEnd, this);
    // responses
    toxav_register_callstate_callback(m_toxAV, callbackAvOnRinging, av_OnRinging, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvOnStarting, av_OnStarting, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvOnEnding, av_OnEnding, this);
    // protocol
    toxav_register_callstate_callback(m_toxAV, callbackAvOnRequestTimeout, av_OnRequestTimeout, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvOnPeerTimeout, av_OnPeerTimeout, this);
    toxav_register_callstate_callback(m_toxAV, callbackAvOnMediaChange, av_OnMediaChange, this);
}

CoreAVModule::~CoreAVModule()
{
    toxav_kill(m_toxAV);
}

void CoreAVModule::update()
{
}

void CoreAVModule::start()
{
}

void CoreAVModule::setAudioInput(QAudioDeviceInfo info)
{
    if (m_audioSource != nullptr)
        delete m_audioSource;

    QAudioFormat format;

    format.setChannelCount(av_DefaultSettings.audio_channels);
    format.setCodec("audio/pcm");
    format.setSampleRate(av_DefaultSettings.audio_sample_rate);
    format.setSampleSize(16);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    m_audioSource = new QAudioInput(info, format, this);

    m_audioSource->setVolume(1.0f);

    m_audioSource->setNotifyInterval(20 /*ms*/);

    connect(m_audioSource, &QAudioInput::notify, this, &CoreAVModule::onAudioInputAvailable);
    m_audioInputBuffer = m_audioSource->start();
}

void CoreAVModule::startCall(int friendnumber, bool withVideo)
{
    QMutexLocker lock(coreMutex());

    int callIndex = 0;
    int ret = toxav_call(m_toxAV, &callIndex, friendnumber, &av_DefaultSettings, TOXAV_RINGING_SECONDS);

    if (ret == 0)
        emit callStarted(friendnumber, callIndex, withVideo);
    else
        qDebug() << "Start Call Error: " << ret;
}

void CoreAVModule::answerCall(int callIndex, bool withVideo)
{
    QMutexLocker lock(coreMutex());

    // the codec we use for our transmission
    ToxAvCSettings answerCodec = av_DefaultSettings;
    answerCodec.call_type = withVideo ? TypeVideo : TypeAudio;

    // answer
    int ret = toxav_answer(m_toxAV, callIndex, &answerCodec);
    if (ret == 0)
    {
        toxav_prepare_transmission(m_toxAV, callIndex, av_jbufdc, av_VADd, withVideo ? 1 : 0);
        emit callAnswered(callIndex, toxav_get_peer_id(m_toxAV, callIndex, 0), withVideo);
    } else
        qDebug() << "Answer Call Error: " << ret;

    //setup audio output format as given by the caller
    ToxAvCSettings inputCodec;
    toxav_get_peer_csettings(m_toxAV, callIndex, 0, &inputCodec);

    QAudioFormat format;
    format.setSampleRate(inputCodec.audio_sample_rate);
    format.setChannelCount(inputCodec.audio_channels);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setCodec("audio/pcm");

    //insert the new call
    m_calls.insert(callIndex, ToxCall::Ptr(new ToxCall(callIndex, 0, format)));
}

void CoreAVModule::hangupCall(int callIndex)
{
    QMutexLocker lock(coreMutex());

    int ret = toxav_hangup(m_toxAV, callIndex);
    if (ret == 0)
        m_calls.remove(callIndex);
    else
        qDebug() << "Hangup Call Error: " << ret;
}

void CoreAVModule::stopCall(int callIndex)
{
    QMutexLocker lock(coreMutex());

    int ret = toxav_stop_call(m_toxAV, callIndex);
    if (ret == 0)
        m_calls.remove(callIndex);
    else
        qDebug() << "Stop Call Error: " << ret;
}

void CoreAVModule::sendVideoFrame(int callIndex, vpx_image* img)
{
    QMutexLocker lock(coreMutex());

    int maxFrameSize = img->w * img->h * 4; //YUVA

    //alloc more space if needed
    if (m_encoderBuffer.size() < maxFrameSize)
        m_encoderBuffer.resize(maxFrameSize);

    //let the encoder do its work
    int frameSize = toxav_prepare_video_frame(m_toxAV, callIndex, U8Ptr(m_encoderBuffer.data()), m_encoderBuffer.size(), img);

    if (frameSize > 0)
        toxav_send_video(m_toxAV, callIndex, U8Ptr(m_encoderBuffer.data()), frameSize);
    else
        qDebug() << "Cannot encode video: " << frameSize;
}

void CoreAVModule::sendAudioFrame(int callIndex, QByteArray frame)
{
    QMutexLocker lock(coreMutex());

    ToxCall::Ptr call = m_calls.value(callIndex);

    int maxFrameSize = m_audioSource->format().bytesPerFrame();

    //alloc more space if needed
    if (m_encoderBuffer.size() < maxFrameSize)
        m_encoderBuffer.resize(maxFrameSize);

    //https://mf4.xiph.org/jenkins/view/opus/job/opus/ws/doc/html/group__opus__encoder.html#gad2d6bf6a9ffb6674879d7605ed073e25
    int frameSize = toxav_prepare_audio_frame(m_toxAV, callIndex,
                                              U8Ptr(m_encoderBuffer.data()),
                                              m_encoderBuffer.size(),
                                              reinterpret_cast<int16_t*>(frame.data()),
                                              frame.size() / m_audioSource->format().bytesPerFrame());

    if (frameSize > 0)
        toxav_send_audio(m_toxAV, callIndex, U8Ptr(m_encoderBuffer.data()), frameSize);
    else
        qDebug() << "Cannot encode audio Error: " << frameSize;
}

void CoreAVModule::onAudioInputAvailable()
{
    QMutexLocker lock(coreMutex());

    int bytesPerFrame = m_audioSource->format().bytesPerFrame();
    int frames = m_audioSource->format().framesForBytes(m_audioInputBuffer->bytesAvailable());

    for (int i=0;i<frames;++i)
    {
        QByteArray input = m_audioInputBuffer->read(bytesPerFrame);

        for (int callIdx : m_calls.keys())
        {
            sendAudioFrame(callIdx, input);
        }
    }
}

/********************
 * CALLBACKS
 ********************/

void CoreAVModule::callbackAudioRecv(ToxAv* toxAV, int32_t call_idx, int16_t* frame, int frame_size, void* userdata)
{
    auto module = static_cast<CoreAVModule*>(userdata);
    ToxCall::Ptr call = module->m_calls.value(call_idx);
    if (!call.isNull())
    {
        QByteArray samples = QByteArray(reinterpret_cast<char*>(frame), frame_size * sizeof(int16_t));
        call->writeAudio(samples);
    }

    Q_UNUSED(toxAV)
}

void CoreAVModule::callbackVideoRecv(ToxAv* toxAV, int32_t call_idx, vpx_image_t* frame, void* userdata)
{
}

void CoreAVModule::callbackAvInvite(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    ToxAvCSettings settings;
    toxav_get_peer_csettings(module->m_toxAV, call_idx, 0, &settings);
    int friendnumber = toxav_get_peer_id(module->m_toxAV, call_idx, 0);
    emit module->callInviteRcv(friendnumber, call_idx, settings.call_type == TypeVideo ? true : false);

    qDebug() << "INVITE: " << call_idx << " id " << friendnumber << " video " << (settings.call_type == TypeVideo ? true : false);
}

void CoreAVModule::callbackAvStart(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    ToxAvCSettings settings;
    toxav_get_peer_csettings(module->m_toxAV, call_idx, 0, &settings);
    int friendnumber = toxav_get_peer_id(module->m_toxAV, call_idx, 0);
    emit module->callStarted(friendnumber, call_idx, settings.call_type == TypeVideo ? true : false);
}

void CoreAVModule::callbackAvCancel(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    emit module->callStopped(toxav_get_peer_id(module->m_toxAV, call_idx, 0), call_idx);
}

void CoreAVModule::callbackAvReject(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    emit module->callStopped(toxav_get_peer_id(module->m_toxAV, call_idx, 0), call_idx);
}

void CoreAVModule::callbackAvEnd(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    emit module->callStopped(toxav_get_peer_id(module->m_toxAV, call_idx, 0), call_idx);
}

void CoreAVModule::callbackAvOnRinging(void* agent, int32_t call_idx, void* arg)
{
}

void CoreAVModule::callbackAvOnStarting(void* agent, int32_t call_idx, void* arg)
{
}

void CoreAVModule::callbackAvOnEnding(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    emit module->callStopped(toxav_get_peer_id(module->m_toxAV, call_idx, 0), call_idx);
}

void CoreAVModule::callbackAvOnRequestTimeout(void* agent, int32_t call_idx, void* arg)
{
}

void CoreAVModule::callbackAvOnPeerTimeout(void* agent, int32_t call_idx, void* arg)
{
}

void CoreAVModule::callbackAvOnMediaChange(void* agent, int32_t call_idx, void* arg)
{
}



