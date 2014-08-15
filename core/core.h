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

#include "io.h"

#define TOX_CONFIG_FILE_NAME "data"

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

    ToxFileTransferInfo getFileTransferInfo(int filenumber);

    void loadConfig(const QString& filename);
    void saveConfig(const QString& filename);
signals:
    // user
    void userIdChanged(QString userId);
    void usernameChanged(QString username);
    void userStatusMessageChanged(QString msg);
    void statusChanged(Status status);

    // friends
    void friendAdded(int friendId, QString username);
    void friendStatusChanged(int friendId, Status status);
    void friendStatusMessageChanged(int friendId, QString msg);
    void friendUsernameChanged(int friendId, QString newName);
    void friendRequestReceived(QString publicKey, QString msg);
    void friendMessageReceived(int friendnumber, QString msg);

    // IO
    void fileTransferRequested(ToxFileTransferInfo status);
    void fileTransferStarted(ToxFileTransferInfo status);
    void fileTransferFeedback(ToxFileTransferInfo status);

public slots:
    void start();
    void deleteLater();

    // user
    void setUserStatusMessage(QString msg);
    void setUserStatus(Status newStatus);

    // friends
    void acceptFriendRequest(QString clientId);
    void sendFriendRequest(QString address, QString msg);
    void removeFriend(int friendnumber);
    void sendMessage(int friendnumber, QString msg);

    // IO
    void sendFile(int friendNumber, QString filename);

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
    void progressFileTransfers();

private:
    Tox* tox;

    QMutex mutex;
    QTimer ticker;

    Status status;
    bool ipV6Enabled;
    QVector<ToxDhtServer> bootstrapServers;
    QMap<int, ToxFileTransfer::Ptr> fileTransfers;
};

#endif // CORE_H
