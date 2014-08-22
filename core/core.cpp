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
#include <QFileInfo>

#include <tox/tox.h>

#define U8Ptr(x) reinterpret_cast<uint8_t*>(x)

/* ====================
 * CORE
 * ====================*/

Core::Core(bool enableIPv6, QVector<ToxDhtServer> dhtServers)
    : QObject(nullptr)
    , m_tox(nullptr)
    , m_ipV6Enabled(enableIPv6)
    , m_bootstrapServers(dhtServers)
    , m_ioModule(nullptr)
    , m_msgModule(nullptr)
    , m_mutex(QMutex::Recursive)
{
    initCore();

    m_ioModule = new CoreIOModule(this, m_tox, &m_mutex);
    m_msgModule = new CoreMessengerModule(this, m_tox, &m_mutex);
}

Core::~Core()
{
    kill();
}

void Core::registerMetaTypes()
{
    qRegisterMetaType<Status>();
    qRegisterMetaType<ToxFileTransferInfo>();
    qRegisterMetaType<ToxPublicKey>();
    qRegisterMetaType<ToxAddress>();
}

void Core::start()
{
    qDebug() << "Core: start";
    connect(&m_ticker, &QTimer::timeout, this, &Core::onTimeout, Qt::DirectConnection);

    m_ticker.setSingleShot(false);
    m_ticker.start(10);

    m_ioModule->start();
    m_msgModule->start();

    bootstrap();
}

void Core::deleteLater()
{
    qDebug() << "Delete later";
}

void Core::onTimeout()
{
    m_ticker.setInterval(qMax(1, int(tox_do_interval(m_tox))));

    toxDo();
    m_ioModule->update();
    m_msgModule->update();
}

void Core::loadConfig(const QString& filename)
{
    QMutexLocker lock(&m_mutex);

    QFile config(filename);
    config.open(QFile::ReadOnly);
    QByteArray configData = config.readAll();
    if (tox_load(m_tox, U8Ptr(configData.data()), configData.size()) == 0)
        qDebug() << "tox_load: success";
    else
        qWarning() << "tox_load: Unable to load config " << filename;
}

void Core::saveConfig(const QString& filename)
{
    QMutexLocker lock(&m_mutex);

    QByteArray configData(tox_size(m_tox), 0);
    tox_save(m_tox, U8Ptr(configData.data()));

    QFile config(filename);
    config.open(QFile::WriteOnly | QFile::Truncate);
    config.write(configData);
}

CoreIOModule *Core::ioModule()
{
    return m_ioModule;
}

CoreMessengerModule *Core::msgModule()
{
    return m_msgModule;
}

void Core::initCore()
{
    QMutexLocker lock(&m_mutex);

    Tox_Options options;
    options.ipv6enabled = m_ipV6Enabled ? 1 : 0;
    options.udp_disabled = 0;
    options.proxy_enabled = 0;

    if ((m_tox = tox_new(&options)) == nullptr)
        qCritical() << "tox_new: Cannot initialize core";
    else
        qDebug() << "tox_new: success";
}

void Core::kill()
{
    QMutexLocker lock(&m_mutex);

    tox_kill(m_tox);
    qDebug() << "tox_kill";
}

void Core::toxDo()
{
    QMutexLocker lock(&m_mutex);

    tox_do(m_tox);
}

void Core::bootstrap()
{
    QMutexLocker lock(&m_mutex);

    ToxDhtServer server = m_bootstrapServers.at(0);

    int ret = tox_bootstrap_from_address(m_tox, server.address.toLatin1().data(), server.port, U8Ptr(server.publicKey.data()));
    if (ret == 1)
        qDebug() << "tox_bootstrap_from_address: " << server.address << ":" << server.port;
    else
        qCritical() << "tox_bootstrap_from_address: cannot resolved address";
}

bool Core::isConnected()
{
    QMutexLocker lock(&m_mutex);

    return tox_isconnected(m_tox) == 1 ? true : false;
}

