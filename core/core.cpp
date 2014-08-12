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

#include "core.h"
#include "settings.h"
#include <QDebug>
#include <QEventLoop>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QVector>
#include <QtEndian>

#include <tox/tox.h>

#define TOXU8(x) reinterpret_cast<uint8_t*>(x)
#define TOXC(x) reinterpret_cast<char*>(x)

/* ====================
 * MAPPING
 * ====================*/

Status mapStatus(uint8_t toxStatus)
{
    switch (toxStatus) {
    case TOX_USERSTATUS_NONE:
        return Online;
    case TOX_USERSTATUS_AWAY:
        return Away;
    case TOX_USERSTATUS_BUSY:
        return Busy;
    case TOX_USERSTATUS_INVALID:
        return Offline;
    }
}

/* ====================
 * CALLBACKS
 * ====================*/
void callbackNameChanged(Tox* tox, int32_t friendnumber, const uint8_t* newname, uint16_t length, void* userdata)
{
    Q_UNUSED(tox);

    Core* core = static_cast<Core*>(userdata);
    QString name = QString::fromUtf8(reinterpret_cast<const char*>(newname), length);
    emit core->friendUsernameChanged(friendnumber, name);
}

void callbackFriendRequest(Tox* tox, const uint8_t* public_key, const uint8_t* data, uint16_t length, void* userdata)
{
    Q_UNUSED(tox);

    Core* core = static_cast<Core*>(userdata);
    QByteArray pubkey(reinterpret_cast<const char*>(public_key), TOX_CLIENT_ID_SIZE);
    QString msg = QString::fromUtf8(reinterpret_cast<const char*>(data), length);
    emit core->friendRequestReceived(pubkey.toHex().toUpper(), msg);
}

void callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata)
{
    Q_UNUSED(tox);

    Core* core = static_cast<Core*>(userdata);
    QString msg = QString::fromUtf8(reinterpret_cast<const char*>(message), length);
    emit core->friendMessageReceived(friendnumber, msg);
}

void callbackFriendAction(Tox* tox, int32_t friendnumber, const uint8_t* action, uint16_t length, void* userdata)
{
    Q_UNUSED(tox);
}

void callbackStatusMessage(Tox* tox, int32_t friendnumber, const uint8_t* newstatus, uint16_t length, void* userdata)
{
    Q_UNUSED(tox);

    Core* core = static_cast<Core*>(userdata);
    QString msg = QString::fromUtf8(reinterpret_cast<const char*>(newstatus), length);
    emit core->friendStatusMessageChanged(friendnumber, msg);
}

void callbackUserStatus(Tox* tox, int32_t friendnumber, uint8_t TOX_USERSTATUS, void* userdata)
{
    Q_UNUSED(tox);

    Core* core = static_cast<Core*>(userdata);
    emit core->friendStatusChanged(friendnumber, mapStatus(TOX_USERSTATUS));
}

void callbackConnectionStatus(Tox* tox, int32_t friendnumber, uint8_t status, void* userdata)
{
    Q_UNUSED(tox);

    Core* core = static_cast<Core*>(userdata);
    emit core->friendStatusChanged(friendnumber, status == 1 ? Online : Offline);

    qDebug() << "Connection status changed " << friendnumber << status;
}

/* ====================
 * CORE
 * ====================*/

Core::Core()
    : QObject(nullptr)
    , tox(nullptr)
    , status(Offline)
{
    initCore();
    loadConfig();
    setupCallbacks();
}

Core::~Core()
{
    kill();
}

int Core::getMaxNameLength()
{
    return TOX_MAX_NAME_LENGTH;
}

void Core::start()
{
    qDebug() << "Core: start";

    ticker.setInterval(1000 / 20); // 20 times per second;
    ticker.setSingleShot(false);
    connect(&ticker, &QTimer::timeout, this, &Core::onTimeout);
    ticker.start();

    bootstrap();
    emit usernameChanged(getUsername());
    queryFriends();
    queryUserId();
    queryUserStatusMessage();
}

void Core::deleteLater()
{
    qDebug() << "Delete later";
}

void Core::onTimeout()
{
    toxDo();

    if (isConnected()) {
        if (status == Offline)
            changeStatus(Online);
    } else {
        changeStatus(Offline);
    }
}

void Core::loadConfig()
{
    QMutexLocker lock(&mutex);

    QFile config(Settings::getSettingsDirPath() + '/' + CONFIG_FILE_NAME);
    config.open(QFile::ReadOnly);
    QByteArray configData = config.readAll();
    if (tox_load(tox, TOXU8(configData.data()), configData.size()) == 0)
        qDebug() << "tox_load: success";
    else
        qWarning() << "tox_load: Unable to load config " << Settings::getSettingsDirPath() + '/' + CONFIG_FILE_NAME;
}

void Core::saveConfig()
{
    QByteArray configData(tox_size(tox), 0);
    tox_save(tox, TOXU8(configData.data()));

    QFile config(Settings::getSettingsDirPath() + '/' + CONFIG_FILE_NAME);
    config.open(QFile::WriteOnly | QFile::Truncate);
    config.write(configData);
}

void Core::initCore()
{
    QMutexLocker lock(&mutex);

    if ((tox = tox_new(Settings::getInstance().getEnableIPv6() ? 1 : 0)) == nullptr)
        qCritical() << "tox_new: Cannot initialize core";
    else
        qDebug() << "tox_new: success";
}

void Core::setupCallbacks()
{
    tox_callback_friend_request(tox, callbackFriendRequest, this);
    tox_callback_friend_message(tox, callbackFriendMessage, this);
    tox_callback_friend_action(tox, callbackFriendAction, this);
    tox_callback_status_message(tox, callbackStatusMessage, this);
    tox_callback_user_status(tox, callbackUserStatus, this);
    tox_callback_connection_status(tox, callbackConnectionStatus, this);
    tox_callback_name_change(tox, callbackNameChanged, this);
}

void Core::kill()
{
    QMutexLocker lock(&mutex);

    tox_kill(tox);
    qDebug() << "tox_kill";
}

void Core::toxDo()
{
    QMutexLocker lock(&mutex);

    tox_do(tox);
}

void Core::queryFriends()
{
    int count = tox_count_friendlist(tox);
    QVector<int> friendlist(count);
    tox_get_friendlist(tox, friendlist.data(), count);

    for (int friendNumber : friendlist) {
        QByteArray nameData(tox_get_name_size(tox, friendNumber), 0);
        if (tox_get_name(tox, friendNumber, TOXU8(nameData.data())) == nameData.length()) {
            qDebug() << "Add friend " << QString::fromUtf8(nameData);
            emit friendAdded(friendNumber, QString::fromUtf8(nameData));
        }
    }
}

void Core::bootstrap()
{
    QMutexLocker lock(&mutex);

    Settings::DhtServer server = Settings::getInstance().getDhtServerList().at(1);

    int ret = tox_bootstrap_from_address(tox, server.address.toLatin1().data(),
                                         Settings::getInstance().getEnableIPv6(),
                                         qToBigEndian(server.port),
                                         TOXU8(server.userId.data()));

    if (ret == 1)
        qDebug() << "tox_bootstrap_from_address: " << server.address << ":" << server.port;
    else
        qCritical() << "tox_bootstrap_from_address: cannot resolved address";
}

bool Core::isConnected()
{
    QMutexLocker lock(&mutex);

    return tox_isconnected(tox) == 1 ? true : false;
}

void Core::queryUserId()
{
    QMutexLocker lock(&mutex);

    QByteArray addressData(TOX_FRIEND_ADDRESS_SIZE, 0);
    tox_get_address(tox, TOXU8(addressData.data()));

    emit userIdChanged(addressData.toHex().toUpper());
}

void Core::queryUserStatusMessage()
{
    QMutexLocker lock(&mutex);

    QByteArray msgData(tox_get_self_status_message_size(tox), 0);
    tox_get_self_status_message(tox, TOXU8(msgData.data()), msgData.length());

    emit userStatusMessageChanged(QString::fromUtf8(msgData));
}

QString Core::getUsername()
{
    QMutexLocker lock(&mutex);

    QByteArray nameData(tox_get_self_name_size(tox), 0);

    if (tox_get_self_name(tox, TOXU8(nameData.data())) == nameData.length()) {
        QString name = QString::fromUtf8(nameData);
        qDebug() << "tox_get_self_name: sucess [" << name << "]";
        return name;
    }

    qDebug() << "tox_get_self_name: failed";
    return "nil";
}

void Core::setUsername(const QString& username)
{
    QMutexLocker lock(&mutex);

    if (tox_set_name(tox, TOXU8(username.toUtf8().data()), username.toUtf8().length()) == 0)
        qDebug() << "tox_set_name: success";
    else
        qDebug() << "tox_set_name: failed";

    emit usernameChanged(username);
}

void Core::changeStatus(Status newStatus)
{
    QMutexLocker lock(&mutex);

    if (status != newStatus) {
        status = newStatus;
        emit statusChanged(status);
    }
}

void Core::acceptFriendRequest(QString clientId)
{
    QMutexLocker lock(&mutex);

    int friendnumber = tox_add_friend_norequest(tox, TOXU8(QByteArray::fromHex(clientId.toLower().toLatin1()).data()));

    if (friendnumber >= 0)
        emit friendAdded(friendnumber, "connecting...");

    qDebug() << "Accept friend request " << clientId << "Result:" << friendnumber;
}

void Core::sendFriendRequest(QString address, QString msg)
{
    QMutexLocker lock(&mutex);

    int friendNumber = tox_add_friend(tox,
                                      TOXU8(QByteArray::fromHex(address.toLower().toLatin1()).data()),
                                      TOXU8(msg.toUtf8().data()),
                                      msg.toUtf8().length());

    emit friendAdded(friendNumber, "connecting...");

    if (friendNumber < 0)
        qDebug() << "Failed sending friend request with code " << friendNumber;
}

void Core::removeFriend(int friendnumber)
{
    QMutexLocker lock(&mutex);

    tox_del_friend(tox, friendnumber);
}

void Core::sendMessage(int friendnumber, QString msg)
{
    QMutexLocker lock(&mutex);

    tox_send_message(tox, friendnumber, TOXU8(msg.toUtf8().data()), msg.toUtf8().size());
}

void Core::setUserStatusMessage(QString msg)
{
    QMutexLocker lock(&mutex);

    tox_set_status_message(tox, TOXU8(msg.toUtf8().data()), msg.toUtf8().size());

    qDebug() << "tox_set_status_message" << msg;
}
