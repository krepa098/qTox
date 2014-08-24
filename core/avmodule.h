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

#include <QtMultimedia/QAudioInput>
#include <QtMultimedia/QAudioBuffer>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

#define TOXAV_MAXCALLS 32
#define TOXAV_RINGING_SECONDS 15
#define TOXAV_VIDEOBUFFER_SIZE 1024 * 1024 * 3

struct _ToxAv;

class ToxCall : public QObject
{
    Q_OBJECT
public:
    using Ptr = QSharedPointer<ToxCall>;
    ToxCall(int callIndex, int friendnumber, QAudioFormat format);

    void writeAudio(const QByteArray& data);

private:
    QAudioFormat m_audioFormat;
    QAudioOutput* m_audioOutput;
    QIODevice* m_audioBuffer;
    int m_callIndex;
    int m_friendnumber;
    int m_state;

};

class CoreAVModule : public CoreModule {
    Q_OBJECT
public:
    CoreAVModule(QObject* parent, Tox* tox, QMutex* mutex);
    ~CoreAVModule();

    void update();
    void start();

    void setAudioInput(QAudioDeviceInfo info);

signals:
    void callStarted(int callIndex, int friendnumber, bool withVideo);
    void callAnswered(int callIndex, int friendnumber, bool withVideo);
    void callHungup(int callIndex, int friendnumber);
    void callStopped(int callIndex, int friendnumber);

    void callInviteRcv(int callIndex, int friendnumber, bool withVideo);

public slots:
    void startCall(int friendnumber, bool withVideo);
    void answerCall(int callIndex, bool withVideo);
    void hangupCall(int callIndex);
    void stopCall(int callIndex);

    void sendVideoFrame(int callIndex, vpx_image* img);
    void sendAudioFrame(int callIndex, QByteArray frame);

private slots:
    void onAudioInputAvailable();

private:
    // callbacks -- userdata is always a pointer to an instance of this class
    // audio & video
    static void callbackAudioRecv(_ToxAv* toxAV, int32_t call_idx, int16_t* frame, int frame_size, void* userdata);
    static void callbackVideoRecv(_ToxAv* toxAV, int32_t call_idx, vpx_image_t* frame, void* userdata);

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
    _ToxAv* m_toxAV;
    QMap<int, ToxCall::Ptr> m_calls;
    QByteArray m_encoderBuffer;

    QAudioInput* m_audioSource;
    QIODevice* m_audioInputBuffer;
    QAudioOutput* m_audioOutput;
};

#endif // AVMODULE_H
