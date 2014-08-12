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

class Core : public QObject
{
    Q_OBJECT
public:
    explicit Core();
    ~Core();

    static int getMaxNameLength();

    QString getUsername();
    void setUsername(const QString& username);

signals:
    void usernameChanged(QString username);

public slots:
    void start();
    void deleteLater();

private slots:
    void onTimeout();

protected:
    // tox wrappers
    void loadConfig();
    void initCore();
    void setupCallbacks();
    void kill();
    void toxDo();


private:
    Tox* tox;

    QMutex mutex;
    QTimer ticker;

};

#endif // CORE_H
