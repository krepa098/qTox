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

#include "msgModule.h"

#include <QMutexLocker>
#include <QDebug>
#include <tox/tox.h>

#define U8Ptr(x) reinterpret_cast<uint8_t*>(x)
#define CPtr(x) reinterpret_cast<const char*>(x)

/********************
 * MAPPING
 ********************/

Status mapStatus(uint8_t toxStatus)
{
    switch (toxStatus) {
    case TOX_USERSTATUS_NONE:
        return Status::Online;
    case TOX_USERSTATUS_AWAY:
        return Status::Away;
    case TOX_USERSTATUS_BUSY:
        return Status::Busy;
    case TOX_USERSTATUS_INVALID:
        return Status::Offline;
    }

    return Status::Offline;
}

/********************
 * ToxGroup
 ********************/

ToxGroup::ToxGroup(int groupnumber)
{
    info.number = groupnumber;
}

bool ToxGroup::update(Tox* tox)
{
    bool updated = false;

    // peer count changed?
    int peerCount = tox_group_number_peers(tox, info.number);
    if (peerCount != info.peerCount) {
        info.peerCount = peerCount;
        info.peers.clear();
        updated = true;
    }

    // query names
    for (int i = 0; i < peerCount; ++i) {
        QByteArray nameData(TOX_MAX_NAME_LENGTH, char(0));
        tox_group_peername(tox, info.number, i, U8Ptr(nameData.data()));

        QString name = QString::fromUtf8(nameData);
        if (info.peers[i] != name) {
            info.peers[i] = name;
            updated = true;
        }
    }

    return updated;
}

/********************
 * CoreMessagingModule
 ********************/

CoreMessengerModule::CoreMessengerModule(QObject* parent, Tox* tox, QMutex* mutex)
    : CoreModule(parent, tox, mutex),
      m_oldStatus(Status::Offline)
{
    // setup callbacks
    tox_callback_friend_request(tox, callbackFriendRequest, this);
    tox_callback_friend_action(tox, callbackFriendAction, this);
    tox_callback_status_message(tox, callbackStatusMessage, this);
    tox_callback_user_status(tox, callbackUserStatus, this);
    tox_callback_connection_status(tox, callbackConnectionStatus, this);
    tox_callback_name_change(tox, callbackNameChanged, this);

    tox_callback_friend_message(tox, callbackFriendMessage, this);
    tox_callback_group_invite(tox, callbackGroupInvite, this);
    tox_callback_group_message(tox, callbackGroupMessage, this);
    tox_callback_group_namelist_change(tox, callbackGroupNamelistChanged, this);
    tox_callback_group_action(tox, callbackGroupAction, this);
}

void CoreMessengerModule::update()
{
    // update group info
    for (ToxGroup& group : m_groups) {
        if (group.update(tox()))
            emit groupInfoAvailable(group.info);
    }
}

void CoreMessengerModule::start()
{
    emit usernameChanged(getUsername());
    emitFriends();
    emitUserStatusMessage();
}

int CoreMessengerModule::getNameMaxLength()
{
    return TOX_MAX_NAME_LENGTH;
}

void CoreMessengerModule::emitFriends()
{
    int count = tox_count_friendlist(tox());
    QVector<int> friendlist(count);
    tox_get_friendlist(tox(), friendlist.data(), count);

    for (int friendNumber : friendlist) {
        QByteArray nameData(tox_get_name_size(tox(), friendNumber), 0);
        if (tox_get_name(tox(), friendNumber, U8Ptr(nameData.data())) == nameData.length()) {
            qDebug() << "Add friend " << QString::fromUtf8(nameData);
            emit friendAdded(friendNumber, QString::fromUtf8(nameData));
        }
    }
}

void CoreMessengerModule::sendMessage(int friendnumber, QString msg)
{
    QMutexLocker lock(coreMutex());

    // TOX_MAX_MESSAGE_LENGTH is a minimum of 342 runes
    QList<QByteArray> splitMsg = CoreHelpers::sliceUTF8After(msg, ' ', TOX_MAX_MESSAGE_LENGTH);

    for (const QByteArray& str : splitMsg)
        tox_send_message(tox(), friendnumber, reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void CoreMessengerModule::emitUserStatusMessage()
{
    QMutexLocker lock(coreMutex());

    QByteArray msgData(tox_get_self_status_message_size(tox()), 0);
    tox_get_self_status_message(tox(), U8Ptr(msgData.data()), msgData.length());

    emit userStatusMessageChanged(QString::fromUtf8(msgData));
}

QString CoreMessengerModule::getUsername()
{
    QMutexLocker lock(coreMutex());

    QByteArray nameData(tox_get_self_name_size(tox()), 0);

    if (tox_get_self_name(tox(), U8Ptr(nameData.data())) == nameData.length()) {
        QString name = QString::fromUtf8(nameData);
        qDebug() << "tox_get_self_name: sucess [" << name << "]";
        return name;
    }

    qDebug() << "tox_get_self_name: failed";
    return "nil";
}

void CoreMessengerModule::setUsername(const QString& username)
{
    QMutexLocker lock(coreMutex());

    if (tox_set_name(tox(), U8Ptr(username.toUtf8().data()), username.toUtf8().length()) == 0)
        qDebug() << "tox_set_name: success";
    else
        qDebug() << "tox_set_name: failed";

    emit usernameChanged(username);
}

ToxAddress CoreMessengerModule::getUserAddress()
{
    QMutexLocker lock(coreMutex());

    ToxAddress address;
    tox_get_address(tox(), U8Ptr(address.data()));

    return address;
}

void CoreMessengerModule::changeStatus(Status newStatus)
{
    if (m_oldStatus != newStatus) {
        m_oldStatus = newStatus;
        emit statusChanged(newStatus);
    }
}

void CoreMessengerModule::acceptFriendRequest(ToxPublicKey friendAddress)
{
    QMutexLocker lock(coreMutex());

    int friendnumber = tox_add_friend_norequest(tox(), U8Ptr(friendAddress.data()));

    if (friendnumber >= 0)
        emit friendAdded(friendnumber, "connecting...");

    qDebug() << "Accept friend request " << friendAddress.toHex().toUpper() << "Result:" << friendnumber;
}

void CoreMessengerModule::sendFriendRequest(ToxAddress address, QString msg)
{
    QMutexLocker lock(coreMutex());

    int friendNumber = tox_add_friend(tox(), address.data(), U8Ptr(msg.toUtf8().data()), msg.toUtf8().length());

    if (friendNumber < 0)
        qDebug() << "Failed sending friend request with code " << friendNumber;
    else
        emit friendAdded(friendNumber, "connecting...");
}

void CoreMessengerModule::removeFriend(int friendnumber)
{
    QMutexLocker lock(coreMutex());

    tox_del_friend(tox(), friendnumber);
}

void CoreMessengerModule::setUserStatusMessage(QString msg)
{
    QMutexLocker lock(coreMutex());

    tox_set_status_message(tox(), U8Ptr(msg.toUtf8().data()), msg.toUtf8().size());

    qDebug() << "tox_set_status_message" << msg;
}

void CoreMessengerModule::setUserStatus(Status newStatus)
{
    QMutexLocker lock(coreMutex());

    tox_set_user_status(tox(), uint8_t(newStatus));
    changeStatus(newStatus);
}

void CoreMessengerModule::acceptGroupInvite(int friendnumber, ToxPublicKey groupPubKey)
{
    // Known bug: we can join a groupchat more than once
    // There is not much we can do about it now

    QMutexLocker lock(coreMutex());

    if (inGroup(groupPubKey))
        return; // already into that group

    int groupnumber = tox_join_groupchat(tox(), friendnumber, U8Ptr(groupPubKey.data()));
    if (groupnumber >= 0) {
        m_groups.insert(groupnumber, ToxGroup(groupnumber));
        emit groupJoined(groupnumber);
    }
}

void CoreMessengerModule::sendGroupInvite(int friendnumber, int groupnumber)
{
    QMutexLocker lock(coreMutex());

    tox_invite_friend(tox(), friendnumber, groupnumber);
}

void CoreMessengerModule::createGroup()
{
    QMutexLocker lock(coreMutex());

    int groupnumber = tox_add_groupchat(tox());
    if (groupnumber >= 0) {
        m_groups.insert(groupnumber, ToxGroup(groupnumber));
        emit groupCreated(groupnumber);
    }
}

void CoreMessengerModule::removeGroup(int groupnumber)
{
    QMutexLocker lock(coreMutex());

    tox_del_groupchat(tox(), groupnumber);
    m_groups.remove(groupnumber);
}

void CoreMessengerModule::sendGroupMessage(int groupnumber, QString msg)
{
    QMutexLocker lock(coreMutex());

    QList<QByteArray> splitMsg = CoreHelpers::sliceUTF8After(msg, ' ', TOX_MAX_MESSAGE_LENGTH);

    for (const QByteArray& str : splitMsg)
        tox_group_message_send(tox(), groupnumber, reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

bool CoreMessengerModule::inGroup(const ToxPublicKey &key) const
{
    for (const ToxGroup& group : m_groups) {
        if (group.info.key == key)
            return true;
    }

    return false;
}

/********************
 * CALLBACKS
 ********************/


void CoreMessengerModule::callbackNameChanged(Tox* tox, int32_t friendnumber, const uint8_t* newname, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    QString name = CoreHelpers::stringFromToxUTF8(newname, length);
    emit module->friendUsernameChanged(friendnumber, name);
}

void CoreMessengerModule::callbackFriendRequest(Tox* tox, const uint8_t* public_key, const uint8_t* data, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    ToxPublicKey pubkey(public_key);
    QString msg = CoreHelpers::stringFromToxUTF8(data, length);
    emit module->friendRequestReceived(pubkey, msg);
}

void CoreMessengerModule::callbackFriendAction(Tox* tox, int32_t friendnumber, const uint8_t* action, uint16_t length, void* userdata)
{
    // TODO: implementation
    Q_UNUSED(tox)
    Q_UNUSED(friendnumber)
    Q_UNUSED(action)
    Q_UNUSED(length)
    Q_UNUSED(userdata)
}

void CoreMessengerModule::callbackStatusMessage(Tox* tox, int32_t friendnumber, const uint8_t* newstatus, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    QString msg = CoreHelpers::stringFromToxUTF8(newstatus, length);
    emit module->friendStatusMessageChanged(friendnumber, msg);
}

void CoreMessengerModule::callbackUserStatus(Tox* tox, int32_t friendnumber, uint8_t TOX_USERSTATUS, void* userdata)
{
    Q_UNUSED(tox)

    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    emit module->friendStatusChanged(friendnumber, mapStatus(TOX_USERSTATUS));
}

void CoreMessengerModule::callbackConnectionStatus(Tox* tox, int32_t friendnumber, uint8_t status, void* userdata)
{
    Q_UNUSED(tox)

    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    emit module->friendStatusChanged(friendnumber, status == 1 ? Status::Online : Status::Offline);

    qDebug() << "Connection status changed " << friendnumber << status;
}

void CoreMessengerModule::callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata)
{
    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    QString msg = CoreHelpers::stringFromToxUTF8(message, length);
    emit module->friendMessageReceived(friendnumber, msg);

    Q_UNUSED(tox)
}

void CoreMessengerModule::callbackGroupInvite(Tox* tox, int friendnumber, const uint8_t* group_public_key, void* userdata)
{
    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    ToxPublicKey pubkey(group_public_key);

    emit module->groupInviteReceived(friendnumber, pubkey);

    Q_UNUSED(tox)
}

void CoreMessengerModule::callbackGroupMessage(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* message, uint16_t length, void* userdata)
{
    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);
    QString msg = CoreHelpers::stringFromToxUTF8(message, length);
    emit module->groupMessage(groupnumber, friendgroupnumber, msg);

    Q_UNUSED(tox)
}

void CoreMessengerModule::callbackGroupNamelistChanged(Tox* tox, int groupnumber, int peer, uint8_t change, void* userdata)
{
    CoreMessengerModule* module = static_cast<CoreMessengerModule*>(userdata);

    QByteArray nameData(TOX_MAX_NAME_LENGTH, char(0));
    tox_group_peername(tox, groupnumber, peer, U8Ptr(nameData.data()));

    // === This turned out to be highly unreliable ===
    switch (change) {
    case TOX_CHAT_CHANGE_PEER_ADD:
        emit module->groupPeerJoined(groupnumber, peer, QString::fromUtf8(nameData));
        break;
    case TOX_CHAT_CHANGE_PEER_DEL:
        emit module->groupPeerLeft(groupnumber, peer, QString::fromUtf8(nameData));
        break;
    case TOX_CHAT_CHANGE_PEER_NAME:
        emit module->groupPeerNameChanged(groupnumber, peer, QString::fromUtf8(nameData));
        break;
    }
}

void CoreMessengerModule::callbackGroupAction(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* action, uint16_t length, void* userdata)
{
    qDebug() << "Group action " << groupnumber << friendgroupnumber;

    Q_UNUSED(tox)
    Q_UNUSED(groupnumber)
    Q_UNUSED(action)
    Q_UNUSED(length)
    Q_UNUSED(userdata)
}
