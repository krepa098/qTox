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

#include "group.h"
#include "widget/groupwidget.h"
#include "widget/form/groupchatform.h"
#include "friendlist.h"
#include "friend.h"
#include "widget/widget.h"
#include "core/core.h"
#include <QDebug>

Group::Group(int GroupId, QString Name)
    : groupId(GroupId)
{
    widget = new GroupWidget(groupId, Name);
    chatForm = new GroupChatForm(this);

    //in groupchats, we only notify on messages containing your name
    hasNewMessages = 0;
    userWasMentioned = 0;
}

Group::~Group()
{
    delete chatForm;
    delete widget;
}

void Group::addPeer(int peerId, QString name)
{
    peers.insert(peerId, name);
    widget->onUserListChanged();
    chatForm->onUserListChanged(peers);
}

void Group::removePeer(int peerId)
{
    peers.remove(peerId);
    widget->onUserListChanged();
    chatForm->onUserListChanged(peers);
}

void Group::updatePeer(int peerId, QString name)
{
    if (peers.contains(peerId))
    {
        peers[peerId] = name;
        widget->onUserListChanged();
        chatForm->onUserListChanged(peers);
    }
}

int Group::peerCount() const
{
    return peers.size();
}

QString Group::peerName(int peer) const
{
    return peers.value(peer, tr("<unknown>"));
}
