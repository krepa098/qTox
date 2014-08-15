#include "io.h"

#include <QFile>

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
    return file.read(maxLen);
}
