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

#ifndef IO_H
#define IO_H

#include <QSharedPointer>
#include <QFile>

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

class ToxFileTransfer
{
public:
    using Ptr = QSharedPointer<ToxFileTransfer>;

    static Ptr createSending(int friendNbr, int fileNbr, QString filename);
    static Ptr createReceiving(int friendNbr, int fileNbr, QString filename, qint64 totalSize);

    ~ToxFileTransfer();

    void setStatus(ToxFileTransferInfo::Status status);
    void setDestination(const QString& path);

    ToxFileTransferInfo getInfo();
    int getFriendnumber() const;
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
#endif // IO_H
