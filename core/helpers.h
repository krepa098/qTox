#ifndef HELPERS_H
#define HELPERS_H

#include <QMetaType>
#include <QByteArray>
#include <QList>
#include <QString>

class CoreHelpers
{
public:
    static QList<QByteArray> sliceUTF8After(const QString &utf8Str,char separator, int maxBytes);
    static QString StringFromToxUTF8(const uint8_t* data, int length);
};

template<int bytes>
class ToxArray
{
public:
    ToxArray() : m_arr(bytes, char(0))
    {

    }

    ToxArray(const uint8_t* data) : m_arr(reinterpret_cast<const char*>(data), bytes)
    {

    }

    QString toHex() const
    {
        return m_arr.toHex().toUpper();
    }

    static ToxArray<bytes> fromHex(QString hex)
    {
        ToxArray<bytes> out;

        QByteArray dat = QByteArray::fromHex(hex.trimmed().toLower().toLatin1());
        if (dat.size() == bytes)
            out.m_arr = dat;

        return out;
    }

    uint8_t* data()
    {
        return reinterpret_cast<uint8_t*>(m_arr.data());
    }

    int size() const
    {
        return m_arr.size();
    }

    bool operator== (const ToxArray<bytes>& other) const
    {
        return m_arr == other.m_arr;
    }

private:
    QByteArray m_arr;
};

// type defs
using ToxPublicKey = ToxArray<32>; // 32 bytes
using ToxAddress = ToxArray<38>; // public key (32) + nospam (4) + checksum (2) bytes

Q_DECLARE_METATYPE(ToxPublicKey)
Q_DECLARE_METATYPE(ToxAddress)

#endif // HELPERS_H
