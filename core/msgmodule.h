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
#include <QStringList>
#include "module.h"
#include "helpers.h"

/********************
 * ToxStatus
 ********************/

enum class ToxStatus : int
{
    Online = 0,
    Away,
    Busy,
    Offline
};

Q_DECLARE_METATYPE(ToxStatus)

/********************
 * ToxGroupInfo
 ********************/

struct ToxGroupInfo {
    int number;
    int peerCount;
    QMap<int, QString> peers;
    ToxPublicKey key;
};

Q_DECLARE_METATYPE(ToxGroupInfo)

/********************
 * ToxGroup
 ********************/

struct ToxGroup {
    ToxGroup(int groupnumber);
    bool update(Tox* tox);

    ToxGroupInfo info;
};

/********************
 * CoreMessengerModule
 ********************/

class CoreMessengerModule : public CoreModule {
    Q_OBJECT
public:
    CoreMessengerModule(Tox* tox, QMutex* mutex, QObject* parent);
    void update();
    void start();

    static int getNameMaxLength();

    ToxAddress getUserAddress();
    QString getUsername();
    ToxStatus getUserStatus();

    void setUsername(const QString& username);

signals:
    // user
    void usernameChanged(QString username);
    void userStatusMessageChanged(QString msg);
    void statusChanged(ToxStatus status);

    // friends
    void friendAdded(int friendnumber, QString username);
    void friendStatusChanged(int friendnumber, ToxStatus status);
    void friendStatusMessageChanged(int friendnumber, QString msg);
    void friendUsernameChanged(int friendnumber, QString newName);
    void friendRequestReceived(ToxPublicKey publicKey, QString msg);
    void friendMessageReceived(int friendnumber, QString msg);

    // group chats
    void groupInviteReceived(int friendnumber, ToxPublicKey groupPubKey);
    void groupMessage(int groupnumber, int friendgroupnumber, QString msg);
    void groupJoined(int groupnumber);
    void groupCreated(int groupnumber);
    void groupInfoAvailable(ToxGroupInfo info);

    //note: Use these for messages like "xy joined the chat" etc.
    //      There is absolutely no guarantee that they are fired in the right order
    //      Use groupInfoAvailable as source of reliable information.
    void groupPeerNameChanged(int groupnumber, int peer, QString name);
    void groupPeerJoined(int groupnumber, int peer, QString name);
    void groupPeerLeft(int groupnumber, int peer, QString name);

public slots:
    // user
    void setUserStatusMessage(QString msg);
    void setUserStatus(ToxStatus newStatus);

    // friends
    void acceptFriendRequest(ToxPublicKey friendAddress);
    void sendFriendRequest(ToxAddress address, QString msg);
    void removeFriend(int friendnumber);
    void sendMessage(int friendnumber, QString msg);

    // group chats
    void acceptGroupInvite(int friendnumber, ToxPublicKey groupPubKey);
    void sendGroupInvite(int friendnumber, int groupnumber);
    void createGroup();
    void removeGroup(int groupnumber);
    void sendGroupMessage(int groupnumber, QString msg);

private:
    // callbacks -- userdata is always a pointer to an instance of this class
    static void callbackNameChanged(Tox* tox, int32_t friendnumber, const uint8_t* newname, uint16_t length, void* userdata);
    static void callbackFriendRequest(Tox* tox, const uint8_t* public_key, const uint8_t* data, uint16_t length, void* userdata);
    static void callbackFriendAction(Tox* tox, int32_t friendnumber, const uint8_t* action, uint16_t length, void* userdata);
    static void callbackStatusMessage(Tox* tox, int32_t friendnumber, const uint8_t* newstatus, uint16_t length, void* userdata);
    static void callbackUserStatus(Tox* tox, int32_t friendnumber, uint8_t TOX_USERSTATUS, void* userdata);
    static void callbackConnectionStatus(Tox* tox, int32_t friendnumber, uint8_t info, void* userdata);

    static void callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata);
    static void callbackGroupInvite(Tox* tox, int friendnumber, const uint8_t* group_public_key, void* userdata);
    static void callbackGroupMessage(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* message, uint16_t length, void* userdata);
    static void callbackGroupNamelistChanged(Tox* tox, int groupnumber, int peer, uint8_t change, void* userdata);
    static void callbackGroupAction(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* action, uint16_t length, void* userdata);

protected:
    bool inGroup(const ToxPublicKey& key) const;
    void emitFriends();
    void emitUserStatusMessage();

    void changeStatus(ToxStatus newStatus);

private:
    QMap<int, ToxGroup> m_groups;
    ToxStatus m_oldStatus;
};

#endif // MSGMODULE_H
