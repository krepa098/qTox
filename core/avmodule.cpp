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

ToxCall::ToxCall(ToxAv* toxAV, int callIndex, int peer)
    : m_toxAV(toxAV)
    , m_audioOutput(nullptr)
    , m_audioDevice(nullptr)
    , m_callIndex(callIndex)
    , m_peer(peer)
{
    qDebug() << "Created new call";

    // setup audio output format as given by the caller
    ToxAvCSettings peerCodec;
    toxav_get_peer_csettings(m_toxAV, callIndex, peer, &peerCodec);

    // start transmission
    toxav_prepare_transmission(m_toxAV, callIndex, av_jbufdc, av_VADd, peerCodec.call_type == TypeVideo ? 1 : 0);
}

ToxCall::~ToxCall()
{
    qDebug() << "Delete call";
    toxav_kill_transmission(m_toxAV, m_callIndex);
}

void ToxCall::startAudioOutput(QAudioDeviceInfo info)
{
    if (m_audioOutput)
        delete m_audioOutput;

    ToxAvCSettings peerCodec;
    toxav_get_peer_csettings(m_toxAV, m_callIndex, m_peer, &peerCodec);

    // out format (has to be pcm, int16, native endian)
    QAudioFormat outputFormat;
    outputFormat.setCodec("audio/pcm");
    outputFormat.setSampleSize(16);
    outputFormat.setSampleType(QAudioFormat::SignedInt); // int16
    outputFormat.setByteOrder(QAudioFormat::LittleEndian); // native
    outputFormat.setSampleRate(peerCodec.audio_sample_rate);
    outputFormat.setChannelCount(peerCodec.audio_channels);

    qDebug() << "creating output samplerate [" << peerCodec.audio_sample_rate << "] channels [" << peerCodec.audio_channels << "]"
             << "duration [" << peerCodec.audio_frame_duration << "] peer " << m_peer;

    int bufferSize = outputFormat.bytesForDuration(peerCodec.audio_frame_duration*1000*32);

    m_audioOutput = new QAudioOutput(info, outputFormat, this);
    m_audioOutput->setCategory("qTox"); //does not work
    m_audioOutput->setBufferSize(bufferSize); // buffer size needs tweaking (latency)
    m_audioDevice = m_audioOutput->start();
}

void ToxCall::writeToOutputDev(const QByteArray &data)
{
    if (m_audioDevice)
        m_audioDevice->write(data);
}

/********************
 * CoreAVModule
 ********************/

CoreAVModule::CoreAVModule(QObject* parent, Tox* tox, QMutex* mutex)
    : CoreModule(parent, tox, mutex)
    , m_toxAV(nullptr)
    , m_audioSource(nullptr)
    , m_audioInputDevice(nullptr)
    , m_audioOutputDeviceInfo(QAudioDeviceInfo::defaultOutputDevice())
    , m_audioInputDeviceInfo(QAudioDeviceInfo::defaultInputDevice())
{
    m_toxAV = toxav_new(tox, TOXAV_MAXCALLS);

    // callbacks ---
    // audio & video
    toxav_register_audio_recv_callback(m_toxAV, callbackAudioRecv, this);
    toxav_register_video_recv_callback(m_toxAV, callbackVideoRecv, this);

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
    // set the default input source
    setAudioInputSource(m_audioInputDeviceInfo);
}

void CoreAVModule::setAudioInputSource(QAudioDeviceInfo info)
{
    // we might want to change the input device at runtime
    if (m_audioSource != nullptr)
        delete m_audioSource;

    // input format (has to be pcm, int16, native endian)
    QAudioFormat format;
    format.setCodec("audio/pcm");
    format.setSampleRate(av_DefaultSettings.audio_sample_rate);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setChannelCount(av_DefaultSettings.audio_channels);

    if (!info.isFormatSupported(format))
    {
        qDebug() << "WARNING: Unsupported input format, using nearest";
        format = info.nearestFormat(format);
    }

    // create input device
    m_audioSource = new QAudioInput(info, format, this);
    m_audioInputDevice = m_audioSource->start();

    // worker, feeds audio samples to tox/opus
    m_audioTimer.disconnect();
    m_audioTimer.setInterval(av_DefaultSettings.audio_frame_duration);
    m_audioTimer.setSingleShot(false);
    connect(&m_audioTimer, &QTimer::timeout, this, &CoreAVModule::onAudioTimerTimeout, Qt::DirectConnection);
    m_audioTimer.start();
}

void CoreAVModule::startCall(int friendnumber, bool withVideo)
{
    QMutexLocker lock(coreMutex());

    int callIndex = 0;
    int ret = toxav_call(m_toxAV, &callIndex, friendnumber, &av_DefaultSettings, TOXAV_RINGING_SECONDS);
    if (ret == 0)
        emit callStarted(friendnumber, callIndex, withVideo);
    else
        qDebug() << "AV: Start Call Error: " << ret;
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
        emit callAnswered(callIndex, withVideo);
    else
        qDebug() << "AV: Answer Call Error: " << ret;
}

void CoreAVModule::hangupCall(int callIndex)
{
    QMutexLocker lock(coreMutex());

    int ret = toxav_hangup(m_toxAV, callIndex);
    if (ret == 0)
        m_calls.remove(callIndex);
    else
    {
        qDebug() << "Hangup Call Error: " << ret;
        stopCall(callIndex);
    }
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

void CoreAVModule::sendAudioFrame(int callIndex, const QByteArray& framedata, int frameSize)
{
    QMutexLocker lock(coreMutex());

    ToxCall::Ptr call = m_calls.value(callIndex);

    if(call.isNull())
        return;

    int encBufferSize = framedata.size() * 2;

    //alloc more space if needed
    if (m_encoderBuffer.size() < encBufferSize)
        m_encoderBuffer.resize(encBufferSize);

    //https://mf4.xiph.org/jenkins/view/opus/job/opus/ws/doc/html/group__opus__encoder.html#gad2d6bf6a9ffb6674879d7605ed073e25
    int encodedFrameSize = toxav_prepare_audio_frame(m_toxAV, callIndex,
                                                     U8Ptr(m_encoderBuffer.data()),
                                                     m_encoderBuffer.size(),
                                                     reinterpret_cast<const int16_t*>(framedata.data()),
                                                     frameSize);

    if (frameSize > 0)
        toxav_send_audio(m_toxAV, callIndex, U8Ptr(m_encoderBuffer.data()), encodedFrameSize);
    else
        qDebug() << "Cannot encode audio Error: " << frameSize;
}

void CoreAVModule::onAudioTimerTimeout()
{
    QMutexLocker lock(coreMutex());

    int bytesPerFrame = m_audioSource->format().bytesForDuration(20*1000); // an opus frame ie. bytes for n ms of audio
    int bytesReady = m_audioSource->bytesReady();
    int opusFrameSize = bytesPerFrame / m_audioSource->format().bytesPerFrame();

    if (bytesReady >= bytesPerFrame)
    {
        QByteArray data = m_audioInputDevice->read(bytesPerFrame);
        for (int callIdx : m_calls.keys())
        {
            sendAudioFrame(callIdx, data, opusFrameSize);
        }
    }
}

void CoreAVModule::addNewCall(int callIndex, int peer)
{
    //create and insert the new call
    ToxCall::Ptr call = ToxCall::Ptr(new ToxCall(m_toxAV, callIndex, peer));
    call->startAudioOutput(m_audioOutputDeviceInfo);

    m_calls.insert(callIndex, call);
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
        call->writeToOutputDev(samples);
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

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvStart(void* agent, int32_t call_idx, void* arg)
{
    qDebug() << "callbackAvStart";
    auto module = static_cast<CoreAVModule*>(arg);

    ToxAvCSettings settings;
    toxav_get_peer_csettings(module->m_toxAV, call_idx, 0, &settings);
    int friendnumber = toxav_get_peer_id(module->m_toxAV, call_idx, 0);
    toxav_prepare_transmission(module->m_toxAV, call_idx, av_jbufdc, av_VADd, settings.call_type == TypeVideo ? 1 : 0);
    emit module->callStarted(friendnumber, call_idx, settings.call_type == TypeVideo ? true : false);

    module->addNewCall(call_idx, 0);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvCancel(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    emit module->callStopped(call_idx);

    module->m_calls.remove(call_idx);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvReject(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    emit module->callStopped(call_idx);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvEnd(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);

    emit module->callStopped(call_idx);

    module->m_calls.remove(call_idx);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvOnRinging(void* agent, int32_t call_idx, void* arg)
{
    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvOnStarting(void* agent, int32_t call_idx, void* arg)
{
    qDebug() << "callbackAvOnStarting";

    auto module = static_cast<CoreAVModule*>(arg);
    module->addNewCall(call_idx, 0);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvOnEnding(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);
    module->m_calls.remove(call_idx);

    emit module->callStopped(call_idx);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvOnRequestTimeout(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);
    module->m_calls.remove(call_idx);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvOnPeerTimeout(void* agent, int32_t call_idx, void* arg)
{
    auto module = static_cast<CoreAVModule*>(arg);
    module->m_calls.remove(call_idx);

    Q_UNUSED(agent)
    Q_UNUSED(arg)
}

void CoreAVModule::callbackAvOnMediaChange(void* agent, int32_t call_idx, void* arg)
{
    Q_UNUSED(agent)
    Q_UNUSED(arg)
}



