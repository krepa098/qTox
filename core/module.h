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

#ifndef MODULE_H
#define MODULE_H

#include <QObject>
#include <QMutex>

struct Tox;

class CoreModule : public QObject
{
    Q_OBJECT
public:
    CoreModule(QObject* parent, Tox* tox, QMutex* mutex)
        : QObject(parent),
          m_tox(tox),
          m_coreMutex(mutex)
    {
    }

    Tox* tox() {
        return m_tox;
    }

    QMutex* coreMutex()
    {
        return m_coreMutex;
    }

    virtual void update() = 0;
    virtual void start() = 0;

private:
    Tox* m_tox;
    QMutex* m_coreMutex;
};

#endif // MODULE_H
