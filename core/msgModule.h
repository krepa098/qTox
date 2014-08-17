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

#ifndef MSGMODULE_H
#define MSGMODULE_H

#include "module.h"

class CoreMessagingModule : public CoreModule
{
    Q_OBJECT
public:
    CoreMessagingModule(QObject* parent, Tox* tox, QMutex* mutex);
    void update();

signals:
    void friendMessageReceived(int friendnumber, QString msg);

public slots:
    void sendMessage(int friendnumber, QString msg);

private:
    // callbacks -- userdata is always a pointer to an instance of this class
    static void callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata);

private:

};

#endif // MSGMODULE_H
