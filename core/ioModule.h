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

#ifndef IOMODULE_H
#define IOMODULE_H

#include <QSharedPointer>
#include <QFile>
#include <QMap>
#include "module.h"

/********************
 * ToxFileTransferInfo
 ********************/

struct ToxFileTransferInfo
{
    enum Status {
        Paused,
        PausedBySender,
        PausedByReceiver,
        Transit,
        Canceled,
        Finished,
    };

    enum Direction {
        Sending,
        Receiving,
        None,
    };

    ToxFileTransferInfo()
        : status(Paused),
          totalSize(0),
          transmittedBytes(0),
          direction(None),
          filenumber(-1),
          friendnumber(-1)
    {}

    ToxFileTransferInfo(int friendNbr, int fileNbr, QString FileName, QString FilePath, qint64 size, Direction dir)
        : status(Paused),
          totalSize(size),
          transmittedBytes(0),
          direction(dir),
          fileName(FileName),
          filePath(FilePath),
          filenumber(fileNbr),
          friendnumber(friendNbr)
    {}

    Status status;
    qint64 totalSize;
    qint64 transmittedBytes;
    Direction direction;
    QString fileName;
    QString filePath;
    int filenumber;
    int friendnumber;

    bool operator == (const ToxFileTransferInfo& other) const {
        return filenumber == other.filenumber && friendnumber == other.friendnumber;
    }

    bool operator != (const ToxFileTransferInfo& other) const {
        return !(*this == other);
    }
};

Q_DECLARE_METATYPE(ToxFileTransferInfo)

/********************
 * ToxFileTransfer
 ********************/

class ToxFileTransfer
{
public:
    using Ptr = QSharedPointer<ToxFileTransfer>;

    static Ptr createSending(int friendNbr, int fileNbr, QString filename);
    static Ptr createReceiving(int friendNbr, int fileNbr, QString filename, qint64 totalSize);

    ~ToxFileTransfer();

    void setStatus(ToxFileTransferInfo::Status status);
    void setDestination(const QString& path);
    void flush();

    ToxFileTransferInfo getInfo();
    bool isValid() const;

    QByteArray read(qint64 offset, qint64 maxLen);
    void unread(qint64 len);
    void write(const QByteArray& data);

protected:
    ToxFileTransfer(int friendNbr, int fileNbr, QString filename, qint64 totalSize, ToxFileTransferInfo::Direction dir);

private:
    QFile file;
    ToxFileTransferInfo info;
    bool valid;
};

/********************
 * CoreFileModule
 ********************/

class CoreIOModule : public CoreModule
{
    Q_OBJECT
public:
    CoreIOModule(QObject* parent, Tox* tox, QMutex* mutex);
    void update();

signals:
    void fileTransferRequested(ToxFileTransferInfo info);
    void fileTransferFeedback(ToxFileTransferInfo info);

public slots:
    void sendFile(int friendnumber, QString filePath);
    void acceptFile(ToxFileTransferInfo info, QString path);
    void killFile(ToxFileTransferInfo info);
    void pauseFile(ToxFileTransferInfo info);
    void resumeFile(ToxFileTransferInfo info);

private:
    // callbacks -- userdata is always a pointer to an instance of this class
    static void callbackFileControl(Tox* tox, int32_t friendnumber, uint8_t receive_send, uint8_t filenumber, uint8_t control_type, const uint8_t* data, uint16_t length, void* userdata);
    static void callbackFileData(Tox *tox, int32_t friendnumber, uint8_t filenumber, const uint8_t *data, uint16_t length, void *userdata);
    static void callbackFileSendRequest(Tox *tox, int32_t friendnumber, uint8_t filenumber, uint64_t filesize, const uint8_t *filename, uint16_t filename_length, void *userdata);

private:
    QMap<int, ToxFileTransfer::Ptr> m_fileTransfers;
};

#endif // IOMODULE_H
