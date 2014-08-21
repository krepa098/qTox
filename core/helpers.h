#ifndef HELPERS_H
#define HELPERS_H

#include <QByteArray>
#include <QList>

class CoreHelpers
{
public:
    static QList<QByteArray> sliceUTF8After(const QString &utf8Str, int bytes);
};

#endif // HELPERS_H
