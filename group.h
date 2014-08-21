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

#ifndef GROUP_H
#define GROUP_H

#include <QList>
#include <QMap>
#include <QTimer>

#define RETRY_PEER_INFO_INTERVAL 500

struct Friend;
class GroupWidget;
class GroupChatForm;

class Group : public QObject
{
    Q_OBJECT
public:
    Group(int GroupId, QString Name);
    ~Group();
    void updatePeers(QMap<int, QString> newPeers);

    int peerCount() const;
    QString peerName(int peer) const;

private:
    QMap<int,QString> peers;

public:
    int groupId;

    GroupWidget* widget;
    GroupChatForm* chatForm;
    int hasNewMessages, userWasMentioned;
};

#endif // GROUP_H
