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

#define CONFIG_FILE_NAME "data"

struct Tox;

struct ToxFile
{
    enum FileDirection {
        SENDING,
        RECEIVING
    };

    int friendId;
    int fileNum;
    QString fileName;
    QString filePath;
    int filesize;
    QFile* file;
    FileDirection direction;
};

enum Status : int
{
    Online = 0,
    Away,
    Busy,
    Offline
};

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

    static int getMaxNameLength();

    QString getUsername();
    void setUsername(const QString& username);

    void loadConfig(const QString& filename);
    void saveConfig(const QString& filename);
signals:
    void userIdChanged(QString userId);
    void usernameChanged(QString username);
    void userStatusMessageChanged(QString msg);
    void friendAdded(int friendId, QString username);
    void statusChanged(Status status);
    void friendStatusChanged(int friendId, Status status);
    void friendStatusMessageChanged(int friendId, QString msg);
    void friendUsernameChanged(int friendId, QString newName);
    void friendRequestReceived(QString publicKey, QString msg);
    void friendMessageReceived(int friendnumber, QString msg);

public slots:
    void start();
    void deleteLater();

    void acceptFriendRequest(QString clientId);
    void sendFriendRequest(QString address, QString msg);
    void removeFriend(int friendnumber);
    void sendMessage(int friendnumber, QString msg);
    void setUserStatusMessage(QString msg);
    void changeStatus(Status newStatus);

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


private:
    Tox* tox;

    QMutex mutex;
    QTimer ticker;

    Status status;
    bool ipV6Enabled;
    QVector<ToxDhtServer> bootstrapServers;

};

#endif // CORE_H
