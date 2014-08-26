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

#ifndef AVMODULE_H
#define AVMODULE_H

#include "module.h"
#include "helpers.h"

#include <vpx/vpx_image.h>
#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QTimer>

#include <QtMultimedia/QAudioInput>
#include <QtMultimedia/QAudioBuffer>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

#define TOXAV_MAXCALLS 32
#define TOXAV_RINGING_SECONDS 15

typedef struct _ToxAv ToxAv;

/********************
 * ToxCodecSettings
 ********************/

struct ToxCodecSettings
{
    // default settings
    ToxCodecSettings()
        : audioFrameDuration(20)
        , audioBitRate(64000)
    {
        audioFormat.setSampleRate(48000);
        audioFormat.setChannelCount(1);

        // do not modify
        audioFormat.setCodec("audio/pcm");
        audioFormat.setSampleSize(16);
        audioFormat.setSampleType(QAudioFormat::SignedInt); // int16
        audioFormat.setByteOrder(QAudioFormat::LittleEndian); // native
    }

    QAudioFormat audioFormat;
    int audioFrameDuration; // ms
    int audioBitRate; // bits/s
};

/********************
 * ToxCall
 ********************/

class ToxCall : public QObject
{
    Q_OBJECT
public:
    using Ptr = QSharedPointer<ToxCall>;
    ToxCall(ToxAv* toxAV, int callIndex, int peer, QObject* parent);
    ~ToxCall();

    void startAudioOutput(QAudioDeviceInfo info);
    void writeToOutputDev(QByteArray data);
    QAudioFormat getAudioOutputFormat() const;

private:
    ToxAv* m_toxAV;
    QAudioOutput* m_audioOutput;
    QIODevice* m_audioDevice;
    int m_callIndex;
    int m_peer;
    int m_state;
};

/********************
 * CoreAVModule
 ********************/

class CoreAVModule : public CoreModule {
    Q_OBJECT
public:
    CoreAVModule(Tox* tox, QMutex* mutex, QObject* parent);
    ~CoreAVModule();

    void update();
    void start();

    void setAudioInputSource(QAudioDeviceInfo info);

signals:
    // local user
    void callStarted(int friendnumber, int callIndex, bool withVideo);
    void callAnswered(int callIndex, bool withVideo);
    void callHungup(int callIndex);
    void callStopped(int callIndex);

    void callInviteRcv(int friendnumber, int callIndex, bool withVideo);

public slots:
    void startCall(int friendnumber, bool withVideo);
    void answerCall(int callIndex, bool withVideo);
    void hangupCall(int callIndex);
    void stopCall(int callIndex);

    void sendVideoFrame(int callIndex, vpx_image* img);
    void sendAudioFrame(int callIndex, const QByteArray &framedata, int frameSize);

private slots:
    void onAudioTimerTimeout();

protected:
    void addNewCall(int callIndex, int peer);

private:
    // callbacks -- userdata is always a pointer to an instance of this class
    // audio & video
    static void callbackAudioRecv(ToxAv* toxAV, int32_t call_idx, int16_t* frame, int frame_size, void* userdata);
    static void callbackVideoRecv(ToxAv* toxAV, int32_t call_idx, vpx_image_t* frame, void* userdata);

    // callstates
    // requests
    static void callbackAvInvite(void* agent, int32_t call_idx, void* arg);
    static void callbackAvStart(void* agent, int32_t call_idx, void* arg);
    static void callbackAvCancel(void* agent, int32_t call_idx, void* arg);
    static void callbackAvReject(void* agent, int32_t call_idx, void* arg);
    static void callbackAvEnd(void* agent, int32_t call_idx, void* arg);
    // responses
    static void callbackAvOnRinging(void* agent, int32_t call_idx, void* arg);
    static void callbackAvOnStarting(void* agent, int32_t call_idx, void* arg);
    static void callbackAvOnEnding(void* agent, int32_t call_idx, void* arg);
    // protocol
    static void callbackAvOnRequestTimeout(void* agent, int32_t call_idx, void* arg);
    static void callbackAvOnPeerTimeout(void* agent, int32_t call_idx, void* arg);
    static void callbackAvOnMediaChange(void* agent, int32_t call_idx, void* arg);

private:
    ToxAv* m_toxAV;
    QMap<int, ToxCall::Ptr> m_calls;
    QByteArray m_encoderBuffer;

    // audio
    QAudioInput* m_audioInput;
    QIODevice* m_audioInputDevice;
    QTimer* m_audioTimer;

    // codec settings
    ToxCodecSettings m_csettings;

    // sources/sinks
    QAudioDeviceInfo m_audioOutputDeviceInfo;
    QAudioDeviceInfo m_audioInputDeviceInfo;
};

#endif // AVMODULE_H
