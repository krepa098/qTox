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

#ifndef FILETRANSFERTWIDGET_H
#define FILETRANSFERTWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>

#include "core/core.h"

struct ToxFile;

class FileTransfertWidget : public QWidget
{
    Q_OBJECT

public:
    FileTransfertWidget(ToxFileTransferInfo Info);

public slots:
    void onFileTransferInfo(ToxFileTransferInfo currInfo);

private slots:
    void cancelTransfer();
    void acceptRecvRequest();
    void pauseResume();

private:
    QString getHumanReadableSize(int size);
    void hideControlsAndDisconnect();

private:
    QLabel *pic, *filename, *size, *speed, *eta;
    QPushButton *topright, *bottomright;
    QProgressBar *progress;
    QHBoxLayout *mainLayout, *textLayout;
    QVBoxLayout *infoLayout, *buttonLayout;
    QWidget* buttonWidget;
    QDateTime lastUpdate;
    long long lastBytesSent;
    ToxFileTransferInfo info;
    QString savePath;
    QString stopFileButtonStylesheet, pauseFileButtonStylesheet, acceptFileButtonStylesheet;
    void paintEvent(QPaintEvent *);
};

#endif // FILETRANSFERTWIDGET_H
