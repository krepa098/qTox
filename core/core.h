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

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QList>

#include "helpers.h"
#include "iomodule.h"
#include "msgmodule.h"
#include "avmodule.h"

struct Tox;

struct ToxDhtServer
{
    QString name;
    QString address;
    quint16 port;
    ToxPublicKey publicKey;
};

class Core : public QObject
{
    Q_OBJECT
public:
    explicit Core(bool enableIPv6, QList<ToxDhtServer> dhtServers);
    ~Core();

    void loadConfig(const QString& filename);
    void saveConfig(const QString& filename);

    CoreIOModule* ioModule();
    CoreMessengerModule* msgModule();
    CoreAVModule* avModule();

signals:
    void connectionStatusChanged(bool connected);

public slots:
    void start();

private slots:
    void onTimeout();

protected:
    // tox helpers
    void initCore();
    void bootstrap();
    bool isConnected();

private:
    Tox* m_tox;

    QTimer m_ticker;

    bool m_lastConnStatus;
    bool m_ipV6Enabled;
    QList<ToxDhtServer> m_dhtServers;

    CoreIOModule* m_ioModule;
    CoreMessengerModule* m_msgModule;
    CoreAVModule* m_avModule;
    QMutex m_mutex;
};

#endif // CORE_H
