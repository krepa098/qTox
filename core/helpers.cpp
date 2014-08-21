#include "helpers.h"
#include <QString>
#include <QDebug>

QList<QByteArray> CoreHelpers::sliceUTF8After(const QString &utf8Str, int bytes)
{
    QByteArray utf8 = utf8Str.toUtf8();
    QList<QByteArray> out;
    int latestStartByte = 0;
    int currStartByte = 0;
    bytes -= 4; // padding

    for (int i=0;i<utf8.size();++i)
    {
        // is utf8 start byte?
        if (utf8[i] & 0xC0 /* 11000000b */)
        {
            latestStartByte = i;

            //figure out how many bytes to skip till the next start byte
            if (utf8[i] & 0xF0 /* 11110000b */) // skip 4 bytes
                i += 3;
            else if (utf8[i] & 0xE0 /* 11100000b */) // skip 3 bytes
                i += 2;
            else if (utf8[i] & 0xD0 /* 11000000b */) // skip 2 bytes
                i += 1;
        }

        if (i - currStartByte >= bytes)
        {
            // we have reached the maximum size in bytes
            out.append(utf8.mid(currStartByte, latestStartByte - currStartByte));
            currStartByte = latestStartByte;
        }
    }

    // add the remaining bits
    out.append(utf8.mid(currStartByte, utf8.size() - currStartByte));

    return out;
}
