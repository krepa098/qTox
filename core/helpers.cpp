#include "helpers.h"

#include <QDebug>

QList<QByteArray> CoreHelpers::sliceUTF8After(const QString& utf8Str, char separator, int maxBytes)
{
    QByteArray utf8 = utf8Str.toUtf8();
    QList<QByteArray> out;
    int latestRune = 0;
    int latestSeperator = -1; // -1: not found
    int offset = 0; // a utf8 start byte or ASCII char
    maxBytes -= 4; // padding

    int i = 0;
    while (i < utf8.size()) {
        // is utf8 start byte?
        if ((utf8[i] & 0xC0) == 0xC0 /* 1100 0000b */) {
            latestRune = i - offset;
        } else if ((utf8[i] & 0x80 /* 1000 0000b */) == 0) { // is ASCII?
            latestRune = i - offset;

            // is separator?
            if (utf8[i] == separator /*ASCII*/)
                latestSeperator = i - offset;
        }

        if (i - offset >= maxBytes) {
            // we have reached the maximum size in bytes
            // so slice it!
            if (latestSeperator > 0) {
                // slice after the seperator
                out.append(utf8.mid(offset, latestSeperator + 1));
                offset += latestSeperator + 1;
            } else {
                // no separator found
                // slice before the latest utf8 start byte
                out.append(utf8.mid(offset, latestRune));
                offset += latestRune;
            }

            latestSeperator = -1;
        }

        i++;
    }

    // add the remaining bits
    out.append(utf8.mid(offset, utf8.size() - offset));

    return out;
}

QString CoreHelpers::StringFromToxUTF8(const uint8_t *data, int length)
{
    return QString::fromUtf8(reinterpret_cast<const char*>(data), length);
}
