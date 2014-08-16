#include "io.h"

#include <QFile>
#include <QFileInfo>
#include <QDebug>

ToxFileTransfer::Ptr ToxFileTransfer::createSending(int friendNbr, int fileNbr, QString filename)
{
    return Ptr(new ToxFileTransfer(friendNbr, fileNbr, filename, -1, ToxFileTransferInfo::Sending));
}

ToxFileTransfer::Ptr ToxFileTransfer::createReceiving(int friendNbr, int fileNbr, QString filename, qint64 totalSize)
{
    return Ptr(new ToxFileTransfer(friendNbr, fileNbr, filename, totalSize, ToxFileTransferInfo::Receiving));
}

ToxFileTransfer::ToxFileTransfer(int friendNbr, int fileNbr, QString filename, qint64 totalSize, ToxFileTransferInfo::Direction dir)
    : valid(false)
{
    if (dir == ToxFileTransferInfo::Sending) {
        file.setFileName(filename);

        if (file.open(QFile::ReadOnly)) {
            valid = true;

            info = ToxFileTransferInfo(friendNbr, fileNbr, QFileInfo(filename).fileName(),  QFileInfo(filename).absoluteFilePath(), file.size(), dir);
        }
    } else {
        info = ToxFileTransferInfo(friendNbr, fileNbr, filename, QString(), totalSize, dir);
    }
}

ToxFileTransfer::~ToxFileTransfer()
{
    qDebug() << "DEL TRANSFER";
}

void ToxFileTransfer::setStatus(ToxFileTransferInfo::Status status)
{
    info.status = status;
}

void ToxFileTransfer::setDestination(const QString &filePath)
{
    if (info.direction == ToxFileTransferInfo::Receiving)
    {
        info.fileName = filePath;
        file.setFileName(filePath);
        valid = file.open(QFile::WriteOnly | QFile::Truncate);
    }
}

ToxFileTransferInfo ToxFileTransfer::getInfo()
{
    return info;
}

bool ToxFileTransfer::isValid() const
{
    return valid;
}

QByteArray ToxFileTransfer::read(qint64 offset, qint64 maxLen)
{
    file.seek(offset);
    QByteArray data = file.read(maxLen);
    info.transmittedBytes += data.size();

    return data;
}

void ToxFileTransfer::unread(qint64 len)
{
    info.transmittedBytes -= len;
}

void ToxFileTransfer::write(const QByteArray& data)
{
    info.transmittedBytes += data.size();
    file.write(data);
}
