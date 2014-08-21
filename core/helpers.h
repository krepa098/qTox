#ifndef HELPERS_H
#define HELPERS_H

#include <QByteArray>
#include <QList>
#include <QString>

class CoreHelpers
{
public:
    static QList<QByteArray> sliceUTF8After(const QString &utf8Str,char separator, int maxBytes);
    static QString StringFromToxUTF8(const uint8_t* data, int length);
};

// aliases
using ToxPublicKey = QByteArray; // 32 bytes
using ToxPublicAddress = QByteArray; // public key (32) + nospam (4) + checksum (2) bytes

#endif // HELPERS_H
