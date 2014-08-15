#include "io.h"

#include <QFile>
#include <QDebug>

ToxFileTransfer::Ptr ToxFileTransfer::create(int friendNbr, int fileNbr, QString filename, ToxFileTransferInfo::Direction dir)
{
    return Ptr(new ToxFileTransfer(friendNbr, fileNbr, filename, dir));
}

ToxFileTransfer::ToxFileTransfer(int friendNbr, int fileNbr, QString filename, ToxFileTransferInfo::Direction dir)
    : valid(false)
{
    file.setFileName(filename);

    if (file.open(dir == ToxFileTransferInfo::Sending ? QFile::ReadOnly : QFile::WriteOnly | QFile::Truncate))
    {
        valid = true;

        info = ToxFileTransferInfo(friendNbr, fileNbr, file.fileName(), file.size(), dir);
    }
}

ToxFileTransfer::~ToxFileTransfer()
{
    qDebug() << "DEL TRANSFER";
}

void ToxFileTransfer::setFileTransferStatus(ToxFileTransferInfo::Status status)
{
    info.status = status;
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

void ToxFileTransfer::write(const QByteArray &data)
{
    info.transmittedBytes += data.size();
    file.write(data);
}
