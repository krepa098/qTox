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

#include <QMap>
#include "module.h"

class CoreMessagingModule : public CoreModule {
    Q_OBJECT
public:
    CoreMessagingModule(QObject* parent, Tox* tox, QMutex* mutex);
    void update();

signals:
    void friendMessageReceived(int friendnumber, QString msg);

    // group chats
    void groupInviteReceived(int friendnumber, QByteArray groupPubKey);
    void groupMessage(int groupnumber, int friendgroupnumber, QString msg);
    void groupJoined(int groupnumber);
    void groupCreated(int groupnumber);
    void groupPeerNameChanged(int groupnumber, int peer, QString name);
    void groupPeerJoined(int groupnumber, int peer, QString name);
    void groupPeerLeft(int groupnumber, int peer);

public slots:
    void sendMessage(int friendnumber, QString msg);

    // group chats
    void acceptGroupInvite(int friendnumber, QByteArray groupPubKey);
    void sendGroupInvite(int friendnumber, int groupnumber);
    void createGroup();
    void removeGroup(int groupnumber);
    void sendGroupMessage(int groupnumber, QString msg);

private:
    // callbacks -- userdata is always a pointer to an instance of this class
    static void callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata);
    static void callbackGroupInvite(Tox* tox, int friendnumber, const uint8_t* group_public_key, void* userdata);
    static void callbackGroupMessage(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* message, uint16_t length, void* userdata);
    static void callbackGroupNamelistChanged(Tox* tox, int groupnumber, int peer, uint8_t change, void* userdata);
    static void callbackGroupAction(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* action, uint16_t length, void* userdata);

private:
    QMap<QByteArray, int> m_invitedGroups;
};

#endif // MSGMODULE_H
