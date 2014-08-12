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

extern "C" {
#include <tox/tox.h>
}

/* ====================
 * CALLBACKS
 * ====================*/
void callbackNameChanged(Tox* tox, int32_t friendNumber, const uint8_t* newname, uint16_t length, void* userData)
{
    Core* core = static_cast<Core*>(userData);
}

void callbackFriendRequest(Tox* tox, const uint8_t* public_key, const uint8_t* data, uint16_t length, void* userdata)
{
}

void callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata)
{
}

void callbackFriendAction(Tox* tox, int32_t friendnumber, const uint8_t* action, uint16_t length, void* userdata)
{
}

void callbackStatusMessage(Tox* tox, int32_t friendnumber, const uint8_t* newstatus, uint16_t length, void* userdata)
{
}

void callbackUserStatus(Tox* tox, int32_t friendnumber, uint8_t TOX_USERSTATUS, void* userdata)
{
}

void callbackConnectionStatus(Tox* tox, int32_t friendnumber, uint8_t status, void* userdata)
{
}

/* ====================
 * CORE
 * ====================*/

Core::Core()
    : QObject(nullptr)
    , tox(nullptr)
{
    initCore();
    loadConfig();

    emit usernameChanged(getUsername());
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
}

void Core::deleteLater()
{
    qDebug() << "Delete later";
}

void Core::onTimeout()
{
    toxDo();
}

void Core::loadConfig()
{
    QMutexLocker lock(&mutex);

    QFile config(Settings::getSettingsDirPath() + '/' + CONFIG_FILE_NAME);
    config.open(QFile::ReadOnly);
    QByteArray configData = config.readAll();
    if (tox_load(tox, reinterpret_cast<uint8_t*>(configData.data()), configData.size()) == 0)
        qDebug() << "tox_load: success";
    else
        qWarning() << "tox_load: Unable to load config " << Settings::getSettingsDirPath() + '/' + CONFIG_FILE_NAME;
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
    tox_callback_friend_message(tox, &callbackFriendMessage, this);
    tox_callback_friend_action(tox, &callbackFriendAction, this);
    tox_callback_status_message(tox, &callbackStatusMessage, this);
    tox_callback_user_status(tox, &callbackUserStatus, this);
    tox_callback_connection_status(tox, &callbackConnectionStatus, this);
    tox_callback_name_change(tox, &callbackNameChanged, this);
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

QString Core::getUsername()
{
    QMutexLocker lock(&mutex);
    qDebug() << "";

    QByteArray nameData(tox_get_self_name_size(tox), 0);

    if (tox_get_self_name(tox, reinterpret_cast<uint8_t*>(nameData.data())) == nameData.length()) {
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

    if (tox_set_name(tox, reinterpret_cast<uint8_t*>(username.toUtf8().data()), username.toUtf8().length()) == 0)
        qDebug() << "tox_set_name: success";
    else
        qDebug() << "tox_set_name: failed";

    emit usernameChanged(username);
}
