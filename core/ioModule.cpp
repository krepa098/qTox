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

#include "ioModule.h"

#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QDebug>
#include <tox/tox.h>

#define U8Ptr(x) reinterpret_cast<uint8_t*>(x)
#define CPtr(x) reinterpret_cast<const char*>(x)

/********************
 * ToxFileTransfer
 ********************/

ToxFileTransfer::Ptr ToxFileTransfer::createSending(int friendNbr, int fileNbr, QString filename)
{
    return Ptr(new ToxFileTransfer(friendNbr, fileNbr, filename, -1, ToxFileTransferInfo::Sending));
}

ToxFileTransfer::Ptr ToxFileTransfer::createReceiving(int friendNbr, int fileNbr, QString filename, qint64 totalSize)
{
    return Ptr(new ToxFileTransfer(friendNbr, fileNbr, filename, totalSize, ToxFileTransferInfo::Receiving));
}

ToxFileTransfer::ToxFileTransfer(int friendNbr, int fileNbr, QString filename, qint64 totalSize, ToxFileTransferInfo::Direction dir)
    : valid(false)
{
    if (dir == ToxFileTransferInfo::Sending) {
        file.setFileName(filename);

        if (file.open(QFile::ReadOnly)) {
            valid = true;

            info = ToxFileTransferInfo(friendNbr, fileNbr, QFileInfo(filename).fileName(), QFileInfo(filename).absoluteFilePath(), file.size(), dir);
        }
    } else {
        info = ToxFileTransferInfo(friendNbr, fileNbr, filename, QString(), totalSize, dir);
    }
}

ToxFileTransfer::~ToxFileTransfer()
{
    qDebug() << "DEL TRANSFER";
}

void ToxFileTransfer::setStatus(ToxFileTransferInfo::Status status)
{
    info.status = status;
}

void ToxFileTransfer::setDestination(const QString& path)
{
    if (info.direction == ToxFileTransferInfo::Receiving) {
        info.filePath = path + '/' + info.fileName;
        file.setFileName(info.filePath);
        valid = file.open(QFile::WriteOnly | QFile::Truncate);
    }
}

void ToxFileTransfer::flush()
{
    file.flush();
}

ToxFileTransferInfo ToxFileTransfer::getInfo()
{
    return info;
}

bool ToxFileTransfer::isValid() const
{
    return valid;
}

QByteArray ToxFileTransfer::read(qint64 offset, qint64 maxLen)
{
    file.seek(offset);
    QByteArray data = file.read(maxLen);
    info.transmittedBytes += data.size();

    return data;
}

void ToxFileTransfer::unread(qint64 len)
{
    info.transmittedBytes -= len;
}

void ToxFileTransfer::write(const QByteArray& data)
{
    info.transmittedBytes += data.size();
    file.write(data);
}

/********************
 * CoreFileModule
 ********************/

CoreIOModule::CoreIOModule(QObject* parent, Tox* tox, QMutex* mutex)
    : CoreModule(parent, tox, mutex)
{
    // setup callbacks
    tox_callback_file_control(tox, callbackFileControl, this);
    tox_callback_file_data(tox, callbackFileData, this);
    tox_callback_file_send_request(tox, callbackFileSendRequest, this);
}

void CoreIOModule::update()
{
    QMutexLocker lock(coreMutex());

    for (int filenumber : m_fileTransfers.keys()) {
        ToxFileTransfer::Ptr transfer = m_fileTransfers.value(filenumber);
        int friendnumber = transfer->getInfo().friendnumber;

        // send new data to the recipient
        if (transfer->getInfo().status == ToxFileTransferInfo::Transit && transfer->getInfo().direction == ToxFileTransferInfo::Sending) {

            int maximumSize = tox_file_data_size(tox(), friendnumber);
            int remainingBytes = tox_file_data_remaining(tox(), friendnumber, filenumber, 0 /*send*/);
            int offset = transfer->getInfo().totalSize - remainingBytes;

            if (remainingBytes > 0) {
                // send data
                QByteArray filedata = transfer->read(offset, maximumSize);
                if (tox_file_send_data(tox(), friendnumber, filenumber, U8Ptr(filedata.data()), filedata.size()) == -1) {
                    transfer->unread(filedata.size());
                    // error (recipient went offline)
                    qDebug() << "TRANS ERROR!";
                    //transfer->setStatus(ToxFileTransferInfo::Canceled);
                }
            } else {
                qDebug() << "TRANS FINISHED";
                // file transmission finished
                transfer->setStatus(ToxFileTransferInfo::Finished);
                tox_file_send_control(tox(), friendnumber, 0, filenumber, TOX_FILECONTROL_FINISHED, nullptr, 0);
            }
        }

        // drop finished and canceled file transfers
        if (transfer->getInfo().status == ToxFileTransferInfo::Finished || transfer->getInfo().status == ToxFileTransferInfo::Canceled) {
            qDebug() << "drop transfer status:" << transfer->getInfo().status;
            m_fileTransfers.remove(transfer->getInfo().filenumber);
        }

        // report progress
        emit fileTransferFeedback(transfer->getInfo());
    }
}

void CoreIOModule::sendFile(int friendnumber, QString filePath)
{
    QMutexLocker lock(coreMutex());

    qDebug() << "Send file " << filePath;
    QFileInfo info(filePath);

    if (info.isReadable()) {
        int filenumber = tox_new_file_sender(tox(), friendnumber, info.size(), U8Ptr(info.fileName().toUtf8().data()), info.fileName().toUtf8().size());
        if (filenumber >= 0) {
            ToxFileTransfer::Ptr trans = ToxFileTransfer::createSending(friendnumber, filenumber, filePath);
            emit fileTransferRequested(trans->getInfo());
            m_fileTransfers.insert(filenumber, trans);
            qDebug() << "New file sender " << filenumber;
        }
    }
}

void CoreIOModule::acceptFile(ToxFileTransferInfo info, QString path)
{
    QMutexLocker lock(coreMutex());

    ToxFileTransfer::Ptr transfer = m_fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        transfer->setDestination(path);
        if (transfer->isValid()) {
            tox_file_send_control(tox(), info.friendnumber, 1, info.filenumber, TOX_FILECONTROL_ACCEPT, nullptr, 0); // accept
            transfer->setStatus(ToxFileTransferInfo::Transit);
        } else {
            tox_file_send_control(tox(), info.friendnumber, 1, info.filenumber, TOX_FILECONTROL_KILL, nullptr, 0); // invalid location -> kill
        }
    }
}

void CoreIOModule::killFile(ToxFileTransferInfo info)
{
    QMutexLocker lock(coreMutex());

    ToxFileTransfer::Ptr transfer = m_fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        int sendReceive = transfer->getInfo().direction == ToxFileTransferInfo::Sending ? 0 : 1;
        tox_file_send_control(tox(), info.friendnumber, sendReceive, info.filenumber, TOX_FILECONTROL_KILL, nullptr, 0);
        transfer->setStatus(ToxFileTransferInfo::Canceled);
    }
}

void CoreIOModule::pauseFile(ToxFileTransferInfo info)
{
    QMutexLocker lock(coreMutex());

    ToxFileTransfer::Ptr transfer = m_fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        if (info.status == ToxFileTransferInfo::Transit) {
            int sendReceive = transfer->getInfo().direction == ToxFileTransferInfo::Sending ? 0 : 1;
            tox_file_send_control(tox(), info.friendnumber, sendReceive, info.filenumber, TOX_FILECONTROL_PAUSE, nullptr, 0);
            transfer->setStatus(ToxFileTransferInfo::Paused);
        }
    }
}

void CoreIOModule::resumeFile(ToxFileTransferInfo info)
{
    QMutexLocker lock(coreMutex());

    ToxFileTransfer::Ptr transfer = m_fileTransfers.value(info.filenumber);

    if (!transfer.isNull()) {
        if (info.status == ToxFileTransferInfo::Paused) {
            int sendReceive = info.direction == ToxFileTransferInfo::Sending ? 0 : 1;
            tox_file_send_control(tox(), info.friendnumber, sendReceive, info.filenumber, TOX_FILECONTROL_ACCEPT, nullptr, 0);
            transfer->setStatus(ToxFileTransferInfo::Transit);
        }
    }
}

/********************
 * CoreFileModule
 * CALLBACKS
 ********************/

void CoreIOModule::callbackFileControl(Tox* tox, int32_t friendnumber, uint8_t receive_send, uint8_t filenumber, uint8_t control_type, const uint8_t* data, uint16_t length, void* userdata)
{
    qDebug() << "FILECTRL" << receive_send << ":" << control_type;
    CoreIOModule* module = static_cast<CoreIOModule*>(userdata);

    ToxFileTransfer::Ptr transf = module->m_fileTransfers.value(filenumber);
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
            transf->flush();
            break;
        }
    }

    emit module->fileTransferFeedback(transf->getInfo());

    Q_UNUSED(tox)
    Q_UNUSED(data)
    Q_UNUSED(length)
    Q_UNUSED(friendnumber)
}

void CoreIOModule::callbackFileData(Tox* tox, int32_t friendnumber, uint8_t filenumber, const uint8_t* data, uint16_t length, void* userdata)
{
    CoreIOModule* module = static_cast<CoreIOModule*>(userdata);

    ToxFileTransfer::Ptr transf = module->m_fileTransfers.value(filenumber);
    if (!transf.isNull()) {
        QByteArray recData(CPtr(data), length);
        transf->write(recData);
    }

    Q_UNUSED(tox)
    Q_UNUSED(friendnumber)
}

void CoreIOModule::callbackFileSendRequest(Tox* tox, int32_t friendnumber, uint8_t filenumber, uint64_t filesize, const uint8_t* filename, uint16_t filename_length, void* userdata)
{
    CoreIOModule* module = static_cast<CoreIOModule*>(userdata);

    QByteArray filenameData(CPtr(filename), filename_length);
    ToxFileTransfer::Ptr trans = ToxFileTransfer::createReceiving(friendnumber, filenumber, QString::fromUtf8(filenameData), filesize);

    module->m_fileTransfers.insert(filenumber, trans);
    emit module->fileTransferRequested(trans->getInfo());

    Q_UNUSED(tox)
}
