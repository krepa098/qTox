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

#ifndef HELPERS_H
#define HELPERS_H

#include <QMetaType>
#include <QByteArray>
#include <QList>
#include <QString>

class CoreHelpers {
public:
    static QList<QByteArray> sliceUTF8After(const QString& utf8Str, char separator, int maxBytes);
    static QString stringFromToxUTF8(const uint8_t* data, int length);
};

template <int bytes>
class ToxArray {
public:
    ToxArray()
        : m_arr(bytes, char(0))
    {
    }

    ToxArray(const uint8_t* data)
        : m_arr(reinterpret_cast<const char*>(data), bytes)
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

    bool operator==(const ToxArray<bytes>& other) const
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
