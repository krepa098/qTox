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
    for(int i=0;i<peerCount;++i)
    {
        QByteArray nameData(TOX_MAX_NAME_LENGTH, char(0));
        tox_group_peername(tox, info.number, i, U8Ptr(nameData.data()));

        QString name = QString::fromUtf8(nameData);
        if (info.peers[i] != name)
        {
            info.peers[i] = name;
            updated = true;
        }
    }

    return updated;
}

/********************
 * CoreMessagingModule
 ********************/

CoreMessagingModule::CoreMessagingModule(QObject* parent, Tox* tox, QMutex* mutex)
    : CoreModule(parent, tox, mutex)
{
    // setup callbacks
    tox_callback_friend_message(tox, callbackFriendMessage, this);
    tox_callback_group_invite(tox, callbackGroupInvite, this);
    tox_callback_group_message(tox, callbackGroupMessage, this);
    tox_callback_group_namelist_change(tox, callbackGroupNamelistChanged, this);
    tox_callback_group_action(tox, callbackGroupAction, this);
}

void CoreMessagingModule::update()
{
    // update group info
    for(ToxGroup& group : m_groups)
    {
        if (group.update(tox()))
            emit groupInfoAvailable(group.info);
    }
}

void CoreMessagingModule::sendMessage(int friendnumber, QString msg)
{
    QMutexLocker lock(coreMutex());

    // TOX_MAX_MESSAGE_LENGTH is a minimum of 342 runes
    QList<QByteArray> splitMsg = CoreHelpers::sliceUTF8After(msg, ' ', TOX_MAX_MESSAGE_LENGTH);

    for (const QByteArray& str : splitMsg)
        tox_send_message(tox(), friendnumber, reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void CoreMessagingModule::acceptGroupInvite(int friendnumber, ToxPublicKey groupPubKey)
{
    // Known bug: we can join a groupchat more than once
    // There is not much we can do about it now

    QMutexLocker lock(coreMutex());

    if (inGroup(groupPubKey))
        return; // already into that group

    int groupnumber = tox_join_groupchat(tox(), friendnumber, U8Ptr(groupPubKey.data()));
    if (groupnumber >= 0)
    {
        m_groups.insert(groupnumber, ToxGroup(groupnumber));
        emit groupJoined(groupnumber);
    }
}

void CoreMessagingModule::sendGroupInvite(int friendnumber, int groupnumber)
{
    QMutexLocker lock(coreMutex());

    tox_invite_friend(tox(), friendnumber, groupnumber);
}

void CoreMessagingModule::createGroup()
{
    QMutexLocker lock(coreMutex());

    int groupnumber = tox_add_groupchat(tox());
    if (groupnumber >= 0) {
        m_groups.insert(groupnumber, ToxGroup(groupnumber));
        emit groupCreated(groupnumber);
    }
}

void CoreMessagingModule::removeGroup(int groupnumber)
{
    QMutexLocker lock(coreMutex());

    tox_del_groupchat(tox(), groupnumber);
    m_groups.remove(groupnumber);
}

void CoreMessagingModule::sendGroupMessage(int groupnumber, QString msg)
{
    QMutexLocker lock(coreMutex());

    QList<QByteArray> splitMsg = CoreHelpers::sliceUTF8After(msg, ' ', TOX_MAX_MESSAGE_LENGTH);

    for (const QByteArray& str : splitMsg)
        tox_group_message_send(tox(), groupnumber, reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

bool CoreMessagingModule::inGroup(ToxPublicKey key) const
{
    for(const ToxGroup& group : m_groups)
    {
        if (group.info.key == key)
            return true;
    }

    return false;
}

/********************
 * CALLBACKS
 ********************/

void CoreMessagingModule::callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata)
{
    CoreMessagingModule* module = static_cast<CoreMessagingModule*>(userdata);
    QString msg = CoreHelpers::StringFromToxUTF8(message, length);
    emit module->friendMessageReceived(friendnumber, msg);

    Q_UNUSED(tox)
}

void CoreMessagingModule::callbackGroupInvite(Tox* tox, int friendnumber, const uint8_t* group_public_key, void* userdata)
{
    CoreMessagingModule* module = static_cast<CoreMessagingModule*>(userdata);
    ToxPublicKey pubkey(CPtr(group_public_key), TOX_CLIENT_ID_SIZE);

    emit module->groupInviteReceived(friendnumber, pubkey);

    Q_UNUSED(tox)
}

void CoreMessagingModule::callbackGroupMessage(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* message, uint16_t length, void* userdata)
{
    CoreMessagingModule* module = static_cast<CoreMessagingModule*>(userdata);
    QString msg = CoreHelpers::StringFromToxUTF8(message, length);
    emit module->groupMessage(groupnumber, friendgroupnumber, msg);

    Q_UNUSED(tox)
}

void CoreMessagingModule::callbackGroupNamelistChanged(Tox* tox, int groupnumber, int peer, uint8_t change, void* userdata)
{
    CoreMessagingModule* module = static_cast<CoreMessagingModule*>(userdata);

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

    QStringList names;
    for(int i=0;i<tox_group_number_peers(tox, groupnumber);i++)
    {
        QByteArray nameData(TOX_MAX_NAME_LENGTH, char(0));
        tox_group_peername(tox, groupnumber, i, U8Ptr(nameData.data()));
        names.append(QString::fromUtf8(nameData));
    }

    qDebug() << "Group " << groupnumber << " Peer " << peer << " name " << QString::fromUtf8(nameData) << " change " << change;
    qDebug() << names;
}

void CoreMessagingModule::callbackGroupAction(Tox* tox, int groupnumber, int friendgroupnumber, const uint8_t* action, uint16_t length, void* userdata)
{
    qDebug() << "Group action " << groupnumber << friendgroupnumber;

    Q_UNUSED(tox)
    Q_UNUSED(groupnumber)
    Q_UNUSED(action)
    Q_UNUSED(length)
    Q_UNUSED(userdata)
}
