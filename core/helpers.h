#ifndef HELPERS_H
#define HELPERS_H

#include <QByteArray>
#include <QList>

class CoreHelpers
{
public:
    static QList<QByteArray> sliceUTF8After(const QString &utf8Str,char separator, int maxBytes);
};

#endif // HELPERS_H
