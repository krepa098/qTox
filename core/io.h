#ifndef IO_H
#define IO_H

#include <QSharedPointer>
#include <QFile>

struct ToxFileTransferInfo
{
    enum Status {
        Finished,
        Canceled,
        NotYetStarted,
        Paused,
        Invalid,
        InTransit,
    };

    enum Direction {
        Sending,
        Receiving,
        None,
    };

    ToxFileTransferInfo()
        : status(Invalid),
          totalSize(0),
          transmittedBytes(0),
          direction(None),
          filenumber(-1),
          friendnumber(-1)
    {}

    ToxFileTransferInfo(int friendNbr, int fileNbr, QString filename, qint64 size, Direction dir)
        : status(NotYetStarted),
          totalSize(size),
          transmittedBytes(0),
          direction(dir),
          file(filename),
          friendnumber(friendNbr),
          filenumber(fileNbr)
    {}

    Status status;
    qint64 totalSize;
    qint64 transmittedBytes;
    Direction direction;
    QString file;
    int filenumber;
    int friendnumber;
};

class ToxFileTransfer
{
public:
    using Ptr = QSharedPointer<ToxFileTransfer>;

    static Ptr create(int friendNbr, int fileNbr, QString filename, ToxFileTransferInfo::Direction dir);
    ToxFileTransfer(int friendNbr, int fileNbr, QString filename, ToxFileTransferInfo::Direction dir);

    ToxFileTransferInfo getInfo();
    int getFriendnumber() const;
    bool isValid() const;
    QByteArray read(qint64 offset, qint64 maxLen);

private:
    QFile file;
    ToxFileTransferInfo info;
    bool valid;
};
#endif // IO_H
