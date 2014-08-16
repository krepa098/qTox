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
    void setDestination(const QString& filePath);

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
