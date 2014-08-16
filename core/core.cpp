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
#include <QDebug>
#include <QEventLoop>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QVector>
#include <QtEndian>
#include <QFileInfo>

#include <tox/tox.h>

#define U8Ptr(x) reinterpret_cast<uint8_t*>(x)
#define CPtr(x) reinterpret_cast<const char*>(x)

/* ====================
 * MAPPING
 * ====================*/

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

/* ====================
 * CALLBACKS
 * ====================*/
void Core::callbackNameChanged(Tox* tox, int32_t friendnumber, const uint8_t* newname, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);
    QString name = QString::fromUtf8(reinterpret_cast<const char*>(newname), length);
    emit core->friendUsernameChanged(friendnumber, name);
}

void Core::callbackFriendRequest(Tox* tox, const uint8_t* public_key, const uint8_t* data, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);
    QByteArray pubkey(reinterpret_cast<const char*>(public_key), TOX_CLIENT_ID_SIZE);
    QString msg = QString::fromUtf8(reinterpret_cast<const char*>(data), length);
    emit core->friendRequestReceived(pubkey.toHex().toUpper(), msg);
}

void Core::callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);
    QString msg = QString::fromUtf8(reinterpret_cast<const char*>(message), length);
    emit core->friendMessageReceived(friendnumber, msg);
}

void Core::callbackFriendAction(Tox* tox, int32_t friendnumber, const uint8_t* action, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)
}

void Core::callbackStatusMessage(Tox* tox, int32_t friendnumber, const uint8_t* newstatus, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);
    QString msg = QString::fromUtf8(reinterpret_cast<const char*>(newstatus), length);
    emit core->friendStatusMessageChanged(friendnumber, msg);
}

void Core::callbackUserStatus(Tox* tox, int32_t friendnumber, uint8_t TOX_USERSTATUS, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);
    emit core->friendStatusChanged(friendnumber, mapStatus(TOX_USERSTATUS));
}

void Core::callbackConnectionStatus(Tox* tox, int32_t friendnumber, uint8_t status, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);
    emit core->friendStatusChanged(friendnumber, status == 1 ? Status::Online : Status::Offline);

    qDebug() << "Connection status changed " << friendnumber << status;
}

void Core::callbackFileControl(Tox* tox, int32_t friendnumber, uint8_t receive_send, uint8_t filenumber, uint8_t control_type, const uint8_t* data, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    qDebug() << "FILECTRL" << receive_send << ":" << control_type;
    Core* core = static_cast<Core*>(userdata);

    ToxFileTransfer::Ptr transf = core->fileTransfers.value(filenumber);
    if (transf.isNull())
        return;

    // we are sending
    if (receive_send == 1) {
        switch (control_type) {
        case TOX_FILECONTROL_ACCEPT:
            // and the recipient accepted (or unpaused) the file -> start transfer
            transf->setStatus(ToxFileTransferInfo::Transit);
            break;
        case TOX_FILECONTROL_PAUSE:
            // and the recipient paused the filetransfer
            transf->setStatus(ToxFileTransferInfo::PausedByReceiver);
            break;
        case TOX_FILECONTROL_KILL:
            // and the recipient canceled the filetransfer
            transf->setStatus(ToxFileTransferInfo::Canceled);
            break;
        }
    }

    // we are receiving the file...
    if (receive_send == 0) {
        switch (control_type) {
        case TOX_FILECONTROL_ACCEPT:
            transf->setStatus(ToxFileTransferInfo::Transit);
            break;
        case TOX_FILECONTROL_PAUSE:
            // and the sender paused the filetransfer
            transf->setStatus(ToxFileTransferInfo::PausedBySender);
            break;
        case TOX_FILECONTROL_KILL:
            // and sender stopped the filetransfer
            transf->setStatus(ToxFileTransferInfo::Canceled);
            break;
        case TOX_FILECONTROL_FINISHED:
            // and sender has sent everything
            transf->setStatus(ToxFileTransferInfo::Finished);
            break;
        }
    }

    emit core->fileTransferFeedback(transf->getInfo());
}

void Core::callbackFileData(Tox* tox, int32_t friendnumber, uint8_t filenumber, const uint8_t* data, uint16_t length, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);

    ToxFileTransfer::Ptr transf = core->fileTransfers.value(filenumber);
    if (!transf.isNull()) {
        QByteArray recData(CPtr(data), length);
        transf->write(recData);
    }
}

void Core::callbackFileSendRequest(Tox* tox, int32_t friendnumber, uint8_t filenumber, uint64_t filesize, const uint8_t* filename, uint16_t filename_length, void* userdata)
{
    Q_UNUSED(tox)

    Core* core = static_cast<Core*>(userdata);

    QByteArray filenameData(CPtr(filename), filename_length);
    ToxFileTransfer::Ptr trans = ToxFileTransfer::createReceiving(friendnumber, filenumber, QString::fromUtf8(filenameData), filesize);

    core->fileTransfers.insert(filenumber, trans);
    emit core->fileTransferRequested(trans->getInfo());
}

/* ====================
 * CORE
 * ====================*/

Core::Core(bool enableIPv6, QVector<ToxDhtServer> dhtServers)
    : QObject(nullptr)
    , tox(nullptr)
    , info(Status::Offline)
    , ipV6Enabled(enableIPv6)
    , bootstrapServers(dhtServers)
{
    initCore();
    setupCallbacks();
}

Core::~Core()
{
    kill();
}

void Core::registerMetaTypes()
{
    qRegisterMetaType<Status>();
    qRegisterMetaType<ToxFileTransferInfo>();
}

int Core::getNameMaxLength()
{
    return TOX_MAX_NAME_LENGTH;
}

void Core::start()
{
    qDebug() << "Core: start";
    connect(&ticker, &QTimer::timeout, this, &Core::onTimeout, Qt::DirectConnection);

    ticker.setSingleShot(false);
    ticker.start(10);

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
    ticker.setInterval(qMax(1, int(tox_do_interval(tox))));

    toxDo();
    progressFileTransfers();

    if (isConnected()) {
        if (info == Status::Offline)
            changeStatus(Status::Online);
    } else {
        changeStatus(Status::Offline);
    }
}

void Core::loadConfig(const QString& filename)
{
    QMutexLocker lock(&mutex);

    QFile config(filename);
    config.open(QFile::ReadOnly);
    QByteArray configData = config.readAll();
    if (tox_load(tox, U8Ptr(configData.data()), configData.size()) == 0)
        qDebug() << "tox_load: success";
    else
        qWarning() << "tox_load: Unable to load config " << filename;
}

void Core::saveConfig(const QString& filename)
{
    QMutexLocker lock(&mutex);

    QByteArray configData(tox_size(tox), 0);
    tox_save(tox, U8Ptr(configData.data()));

    QFile config(filename);
    config.open(QFile::WriteOnly | QFile::Truncate);
    config.write(configData);
}

void Core::initCore()
{
    QMutexLocker lock(&mutex);

    if ((tox = tox_new(ipV6Enabled ? 1 : 0)) == nullptr)
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
    tox_callback_file_control(tox, callbackFileControl, this);
    tox_callback_file_data(tox, callbackFileData, this);
    tox_callback_file_send_request(tox, callbackFileSendRequest, this);
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
        if (tox_get_name(tox, friendNumber, U8Ptr(nameData.data())) == nameData.length()) {
            qDebug() << "Add friend " << QString::fromUtf8(nameData);
            emit friendAdded(friendNumber, QString::fromUtf8(nameData));
        }
    }
}

void Core::bootstrap()
{
    QMutexLocker lock(&mutex);

    ToxDhtServer server = bootstrapServers.at(0);

    int ret = tox_bootstrap_from_address(tox, server.address.toLatin1().data(),
                                         ipV6Enabled ? 1 : 0,
                                         qToBigEndian(server.port),
                                         U8Ptr(server.publicKey.data()));

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
    tox_get_address(tox, U8Ptr(addressData.data()));

    emit userIdChanged(addressData.toHex().toUpper());
}

void Core::queryUserStatusMessage()
{
    QMutexLocker lock(&mutex);

    QByteArray msgData(tox_get_self_status_message_size(tox), 0);
    tox_get_self_status_message(tox, U8Ptr(msgData.data()), msgData.length());

    emit userStatusMessageChanged(QString::fromUtf8(msgData));
}

QString Core::getUsername()
{
    QMutexLocker lock(&mutex);

    QByteArray nameData(tox_get_self_name_size(tox), 0);

    if (tox_get_self_name(tox, U8Ptr(nameData.data())) == nameData.length()) {
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

    if (tox_set_name(tox, U8Ptr(username.toUtf8().data()), username.toUtf8().length()) == 0)
        qDebug() << "tox_set_name: success";
    else
        qDebug() << "tox_set_name: failed";

    emit usernameChanged(username);
}

void Core::changeStatus(Status newStatus)
{
    if (info != newStatus) {
        info = newStatus;
        emit statusChanged(info);
    }
}

void Core::progressFileTransfers()
{
    QMutexLocker lock(&mutex);

    for (int filenumber : fileTransfers.keys()) {
        ToxFileTransfer::Ptr transfer = fileTransfers.value(filenumber);
        int friendnumber = transfer->getInfo().friendnumber;

        // send new data to the recipient
        if (transfer->getInfo().status == ToxFileTransferInfo::Transit && transfer->getInfo().direction == ToxFileTransferInfo::Sending) {

            int maximumSize = tox_file_data_size(tox, friendnumber);
            int remainingBytes = tox_file_data_remaining(tox, friendnumber, filenumber, 0 /*send*/);
            int offset = transfer->getInfo().totalSize - remainingBytes;

            if (remainingBytes > 0) {
                // send data
                QByteArray filedata = transfer->read(offset, maximumSize);
                if (tox_file_send_data(tox, friendnumber, filenumber, U8Ptr(filedata.data()), filedata.size()) == -1) {
                    transfer->unread(filedata.size());
                    // error (recipient went offline)
                    qDebug() << "TRANS ERROR!";
                    //transfer->setStatus(ToxFileTransferInfo::Canceled);
                }
            } else {
                qDebug() << "TRANS FINISHED";
                // file transmission finished
                transfer->setStatus(ToxFileTransferInfo::Finished);
                tox_file_send_control(tox, friendnumber, 0, filenumber, TOX_FILECONTROL_FINISHED, nullptr, 0);
            }
        }

        // drop finished and canceled file transfers
        if (transfer->getInfo().status == ToxFileTransferInfo::Finished || transfer->getInfo().status == ToxFileTransferInfo::Canceled) {
            qDebug() << "drop transfer status:" << transfer->getInfo().status;
            fileTransfers.remove(transfer->getInfo().filenumber);
        }

        // report progress
        emit fileTransferFeedback(transfer->getInfo());
    }
}

void Core::acceptFriendRequest(QString clientId)
{
    QMutexLocker lock(&mutex);

    int friendnumber = tox_add_friend_norequest(tox, U8Ptr(QByteArray::fromHex(clientId.toLower().toLatin1()).data()));

    if (friendnumber >= 0)
        emit friendAdded(friendnumber, "connecting...");

    qDebug() << "Accept friend request " << clientId << "Result:" << friendnumber;
}

void Core::sendFriendRequest(QString address, QString msg)
{
    QMutexLocker lock(&mutex);

    int friendNumber = tox_add_friend(tox,
                                      U8Ptr(QByteArray::fromHex(address.toLower().toLatin1()).data()),
                                      U8Ptr(msg.toUtf8().data()),
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

    tox_send_message(tox, friendnumber, U8Ptr(msg.toUtf8().data()), msg.toUtf8().size());
}

void Core::sendFile(int friendNumber, QString filePath)
{
    QMutexLocker lock(&mutex);

    qDebug() << "Send file " << filePath;
    QFileInfo info(filePath);

    if (info.isReadable()) {
        int filenumber = tox_new_file_sender(tox, friendNumber, info.size(), U8Ptr(info.fileName().toUtf8().data()), info.fileName().toUtf8().size());
        if (filenumber >= 0) {
            ToxFileTransfer::Ptr trans = ToxFileTransfer::createSending(friendNumber, filenumber, filePath);
            emit fileTransferRequested(trans->getInfo());
            fileTransfers.insert(filenumber, trans);
            qDebug() << "New file sender " << filenumber;
        }
    }
}

void Core::acceptFile(ToxFileTransferInfo info, QString path)
{
    QMutexLocker lock(&mutex);

    ToxFileTransfer::Ptr transfer = fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        transfer->setDestination(path);
        if (transfer->isValid())
        {
            tox_file_send_control(tox, info.friendnumber, 1, info.filenumber, TOX_FILECONTROL_ACCEPT, nullptr, 0); // accept
            transfer->setStatus(ToxFileTransferInfo::Transit);
        }
        else
        {
            tox_file_send_control(tox, info.friendnumber, 1, info.filenumber, TOX_FILECONTROL_KILL, nullptr, 0); // invalid location -> kill
        }
    }
}

void Core::killFile(ToxFileTransferInfo info)
{
    QMutexLocker lock(&mutex);

    ToxFileTransfer::Ptr transfer = fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        int sendReceive = transfer->getInfo().direction == ToxFileTransferInfo::Sending ? 0 : 1;
        tox_file_send_control(tox, info.friendnumber, sendReceive, info.filenumber, TOX_FILECONTROL_KILL, nullptr, 0);
        transfer->setStatus(ToxFileTransferInfo::Canceled);
    }
}

void Core::pauseFile(ToxFileTransferInfo info)
{
    QMutexLocker lock(&mutex);

    ToxFileTransfer::Ptr transfer = fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        if (info.status == ToxFileTransferInfo::Transit)
        {
            int sendReceive = transfer->getInfo().direction == ToxFileTransferInfo::Sending ? 0 : 1;
            tox_file_send_control(tox, info.friendnumber, sendReceive, info.filenumber, TOX_FILECONTROL_PAUSE, nullptr, 0);
            transfer->setStatus(ToxFileTransferInfo::Paused);
            qDebug() << "TOX pause" << sendReceive;
        }
    }
}

void Core::resumeFile(ToxFileTransferInfo info)
{
    QMutexLocker lock(&mutex);

    ToxFileTransfer::Ptr transfer = fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        if (info.status == ToxFileTransferInfo::Paused)
        {
            int sendReceive = info.direction == ToxFileTransferInfo::Sending ? 0 : 1;
            tox_file_send_control(tox, info.friendnumber, sendReceive, info.filenumber, TOX_FILECONTROL_ACCEPT, nullptr, 0);
            transfer->setStatus(ToxFileTransferInfo::Transit);
            qDebug() << "TOX resume" << sendReceive;
        }
    }
}

void Core::setUserStatusMessage(QString msg)
{
    QMutexLocker lock(&mutex);

    tox_set_status_message(tox, U8Ptr(msg.toUtf8().data()), msg.toUtf8().size());

    qDebug() << "tox_set_status_message" << msg;
}

void Core::setUserStatus(Status newStatus)
{
    QMutexLocker lock(&mutex);

    tox_set_user_status(tox, uint8_t(newStatus));
    changeStatus(newStatus);
}
