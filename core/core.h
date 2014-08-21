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

#ifndef CORE_H
#define CORE_H

#include <QFile>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QMap>

#include "helpers.h"
#include "ioModule.h"
#include "msgModule.h"

struct Tox;

enum class Status : int
{
    Online = 0,
    Away,
    Busy,
    Offline
};

Q_DECLARE_METATYPE(Status)

struct ToxDhtServer
{
    QString address;
    quint16 port;
    QString publicKey;
};

class Core : public QObject
{
    Q_OBJECT
public:
    explicit Core(bool enableIPv6, QVector<ToxDhtServer> dhtServers);
    ~Core();

    static void registerMetaTypes();
    static int getNameMaxLength();

    QString getUsername();
    void setUsername(const QString& username);
    ToxAddress getAddress();

    void loadConfig(const QString& filename);
    void saveConfig(const QString& filename);

    CoreIOModule* ioModule();
    CoreMessagingModule* msgModule();

signals:
    // user
    void userIdChanged(QString userId);
    void usernameChanged(QString username);
    void userStatusMessageChanged(QString msg);
    void statusChanged(Status status);

    // friends
    void friendAdded(int friendnumber, QString username);
    void friendStatusChanged(int friendnumber, Status status);
    void friendStatusMessageChanged(int friendnumber, QString msg);
    void friendUsernameChanged(int friendnumber, QString newName);
    void friendRequestReceived(ToxPublicKey publicKey, QString msg);

public slots:
    void start();
    void deleteLater();

    // user
    void setUserStatusMessage(QString msg);
    void setUserStatus(Status newStatus);

    // friends
    void acceptFriendRequest(ToxPublicKey friendAddress);
    void sendFriendRequest(ToxAddress address, QString msg);
    void removeFriend(int friendnumber);

private slots:
    void onTimeout();

protected:
    // tox wrappers
    void initCore();
    void setupCallbacks();
    void kill();
    void toxDo();
    void queryFriends();
    void bootstrap();
    bool isConnected();
    void queryUserId();
    void queryUserStatusMessage();

    void changeStatus(Status newStatus);

private:
    // callbacks -- userdata is always a pointer to an instance of Core
    static void callbackNameChanged(Tox* tox, int32_t friendnumber, const uint8_t* newname, uint16_t length, void* userdata);
    static void callbackFriendRequest(Tox* tox, const uint8_t* public_key, const uint8_t* data, uint16_t length, void* userdata);
    static void callbackFriendAction(Tox* tox, int32_t friendnumber, const uint8_t* action, uint16_t length, void* userdata);
    static void callbackStatusMessage(Tox* tox, int32_t friendnumber, const uint8_t* newstatus, uint16_t length, void* userdata);
    static void callbackUserStatus(Tox* tox, int32_t friendnumber, uint8_t TOX_USERSTATUS, void* userdata);
    static void callbackConnectionStatus(Tox* tox, int32_t friendnumber, uint8_t info, void* userdata);

private:
    Tox* tox;

    QTimer ticker;

    Status info;
    bool ipV6Enabled;
    QVector<ToxDhtServer> bootstrapServers;

    CoreIOModule* m_ioModule;
    CoreMessagingModule* m_msgModule;
    QMutex mutex;
};

#endif // CORE_H
