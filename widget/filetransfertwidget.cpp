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

#include "filetransfertwidget.h"
#include "widget.h"
#include "core/core.h"
#include "math.h"
#include "style.h"
#include <QFileDialog>
#include <QPixmap>
#include <QPainter>
#include <QMessageBox>

FileTransfertWidget::FileTransfertWidget(ToxFileTransferInfo Info)
    : lastUpdate(QDateTime::currentDateTime())
    , lastBytesSent(0)
    , info(Info)
{
    setObjectName("default");
    setStyleSheet(Style::get(":/ui/fileTransferWidget/fileTransferWidget.css"));

    pic = new QLabel(), filename = new QLabel(this), size = new QLabel(this), speed = new QLabel(this), eta = new QLabel(this);
    topright = new QPushButton(), bottomright = new QPushButton();
    progress = new QProgressBar();
    mainLayout = new QHBoxLayout(), textLayout = new QHBoxLayout();
    infoLayout = new QVBoxLayout(), buttonLayout = new QVBoxLayout();
    buttonWidget = new QWidget();
    QFont prettysmall;
    prettysmall.setPixelSize(10);

    setMinimumSize(250, 58);
    setMaximumHeight(58);
    setLayout(mainLayout);
    mainLayout->setMargin(0);

    pic->setMaximumHeight(40);
    pic->setContentsMargins(5, 0, 0, 0);
    filename->setText(info.fileName);
    filename->setFont(prettysmall);
    size->setText(getHumanReadableSize(info.totalSize));
    size->setFont(prettysmall);
    speed->setText("0B/s");
    speed->setFont(prettysmall);
    eta->setText("00:00");
    eta->setFont(prettysmall);
    progress->setValue(0);
    progress->setMinimumHeight(11);
    progress->setFont(prettysmall);
    progress->setTextVisible(false);
    QPalette whitebg;
    whitebg.setColor(QPalette::Window, QColor(255, 255, 255));
    //buttonWidget->setPalette(whitebg);
    buttonWidget->setAutoFillBackground(true);
    buttonWidget->setLayout(buttonLayout);

    stopFileButtonStylesheet = Style::get(":/ui/stopFileButton/style.css");
    pauseFileButtonStylesheet = Style::get(":/ui/pauseFileButton/style.css");
    acceptFileButtonStylesheet = Style::get(":/ui/acceptFileButton/style.css");

    topright->setStyleSheet(stopFileButtonStylesheet);
    if (info.direction == ToxFileTransferInfo::Sending) {
        bottomright->setStyleSheet(pauseFileButtonStylesheet);
        connect(topright, SIGNAL(clicked()), this, SLOT(cancelTransfer()));
        connect(bottomright, SIGNAL(clicked()), this, SLOT(pauseResume()));

        QPixmap preview;
        //        File.file->seek(0);
        //        if (preview.loadFromData(File.file->readAll()))
        //        {
        //            preview = preview.scaledToHeight(40);
        //            pic->setPixmap(preview);
        //        }
        //        File.file->seek(0);
    } else {
        bottomright->setStyleSheet(acceptFileButtonStylesheet);
        connect(topright, SIGNAL(clicked()), this, SLOT(cancelTransfer()));
        connect(bottomright, SIGNAL(clicked()), this, SLOT(acceptRecvRequest()));
    }

    QPalette toxgreen;
    toxgreen.setColor(QPalette::Button, QColor(107, 194, 96)); // Tox Green
    topright->setIconSize(QSize(10, 10));
    topright->setMinimumSize(25, 28);
    topright->setFlat(true);
    topright->setAutoFillBackground(true);
    topright->setPalette(toxgreen);
    bottomright->setIconSize(QSize(10, 10));
    bottomright->setMinimumSize(25, 28);
    bottomright->setFlat(true);
    bottomright->setAutoFillBackground(true);
    bottomright->setPalette(toxgreen);

    mainLayout->addStretch();
    mainLayout->addWidget(pic);
    mainLayout->addLayout(infoLayout, 3);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonWidget);
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);

    infoLayout->addWidget(filename);
    infoLayout->addLayout(textLayout);
    infoLayout->addWidget(progress);
    infoLayout->setMargin(4);
    infoLayout->setSpacing(4);

    textLayout->addWidget(size);
    textLayout->addStretch(0);
    textLayout->addWidget(speed);
    textLayout->addStretch(0);
    textLayout->addWidget(eta);
    textLayout->setMargin(2);
    textLayout->setSpacing(5);

    buttonLayout->addWidget(topright);
    buttonLayout->addSpacing(2);
    buttonLayout->addWidget(bottomright);
    buttonLayout->setContentsMargins(2, 0, 0, 0);
    buttonLayout->setSpacing(0);

    connect(Widget::getInstance()->getCore(), &Core::fileTransferFeedback, this, &FileTransfertWidget::onFileTransferInfo);
}

QString FileTransfertWidget::getHumanReadableSize(int size)
{
    static const char* suffix[] = { "B", "kiB", "MiB", "GiB", "TiB" };
    int exp = 0;
    if (size)
        exp = std::min((int)(log(size) / log(1024)), (int)(sizeof(suffix) / sizeof(suffix[0]) - 1));
    return QString().setNum(size / pow(1024, exp), 'f', 2).append(suffix[exp]);
}

void FileTransfertWidget::hideControlsAndDisconnect()
{
    //disconnect();
    disconnect(Widget::getInstance()->getCore(), &Core::fileTransferFeedback, this, &FileTransfertWidget::onFileTransferInfo);

    progress->hide();
    speed->hide();
    eta->hide();
    topright->hide();
    bottomright->hide();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
}

void FileTransfertWidget::onFileTransferInfo(ToxFileTransferInfo currInfo)
{
    if (currInfo != info)
        return;

    info = currInfo;

    switch (currInfo.status) {
    case ToxFileTransferInfo::Finished:
        setObjectName("success");
        hideControlsAndDisconnect();
        break;
    case ToxFileTransferInfo::Canceled:
        setObjectName("error");
        hideControlsAndDisconnect();
        break;
    case ToxFileTransferInfo::Paused:
    case ToxFileTransferInfo::PausedBySender:
    case ToxFileTransferInfo::PausedByReceiver:
        setObjectName("paused");
        break;
    case ToxFileTransferInfo::Transit:
        setObjectName("default");
        break;
    }

    //reevaluate style
    setStyleSheet(QString());
    setStyleSheet(Style::get(":/ui/fileTransferWidget/fileTransferWidget.css"));

    // calculate progress, speed etc.
    QDateTime newtime = QDateTime::currentDateTime();
    int timediff = lastUpdate.secsTo(newtime);
    if (timediff <= 0)
        return;
    qint64 diff = currInfo.transmittedBytes - lastBytesSent;
    if (diff < 0) {
        qWarning() << "FileTransfertWidget::onFileTransferInfo: Negative transfer speed !";
        diff = 0;
    }
    int rawspeed = diff / timediff;
    speed->setText(getHumanReadableSize(rawspeed) + "/s");
    size->setText(getHumanReadableSize(currInfo.totalSize));
    if (!rawspeed)
        return;
    int etaSecs = (currInfo.totalSize - currInfo.transmittedBytes) / rawspeed;
    QTime etaTime(0, 0);
    etaTime = etaTime.addSecs(etaSecs);
    eta->setText(etaTime.toString("mm:ss"));
    if (!currInfo.totalSize)
        progress->setValue(0);
    else
        progress->setValue(currInfo.transmittedBytes * 100 / currInfo.totalSize);
    qDebug() << QString("FT: received %1/%2 bytes, progress is %3%").arg(currInfo.transmittedBytes).arg(currInfo.totalSize).arg(currInfo.transmittedBytes * 100 / currInfo.totalSize);
    lastUpdate = newtime;
    lastBytesSent = currInfo.transmittedBytes;
}

//void FileTransfertWidget::onFileTransferFinished(ToxFileTransferInfo File)
//{
//    if (File.filenumber != fileNum || File.friendnumber != friendId || File.direction != direction)
//        return;
//    topright->disconnect();
//    disconnect(Widget::getInstance()->getCore(),0,this,0);

//    if (File.direction == ToxFileTransferInfo::Receiving)
//    {
//        QPixmap preview;
//        //        QFile previewFile(File.filePath);
//        //        if (previewFile.open(QIODevice::ReadOnly) && previewFile.size() <= 1024*1024*25) // Don't preview big (>25MiB) images
//        //        {
//        //            if (preview.loadFromData(previewFile.readAll()))
//        //            {
//        //                preview = preview.scaledToHeight(40);
//        //                pic->setPixmap(preview);
//        //            }
//        //            previewFile.close();
//        //        }
//    }
//}

void FileTransfertWidget::cancelTransfer()
{
    Widget::getInstance()->getCore()->killFile(info);
}

// for whatever the fuck reason, QFileInfo::isWritable() always fails for files that don't exist
// which makes it useless for our case
// since QDir doesn't have an isWritable(), the only option I can think of is to make/delete the file
// surely this is a common problem that has a qt-implemented solution?
bool isWritable(QString& path)
{
    QFile file(path);
    bool exists = file.exists();
    bool out = file.open(QIODevice::WriteOnly);
    file.close();
    if (!exists)
        file.remove();
    return out;
}

void FileTransfertWidget::acceptRecvRequest()
{
    QString path = QFileDialog::getSaveFileName(nullptr, tr("Save a file", "Title of the file saving dialog"), QDir::currentPath() + '/' + info.fileName);
    if (!path.isEmpty() && isWritable(path)) {
        Widget::getInstance()->getCore()->acceptFile(info, path);

        bottomright->setStyleSheet(pauseFileButtonStylesheet);
        bottomright->disconnect();
        connect(bottomright, &QPushButton::clicked, this, &FileTransfertWidget::pauseResume);
    } else {
        QMessageBox::warning(this, tr("Location not writable", "Title of permissions popup"), tr("You do not have permission to write that location. Choose another, or cancel the save dialog.", "text of permissions popup"));
    }
}

void FileTransfertWidget::pauseResume()
{
    if (info.status == ToxFileTransferInfo::Paused)
        Widget::getInstance()->getCore()->resumeFile(info);

    if (info.status == ToxFileTransferInfo::Transit)
        Widget::getInstance()->getCore()->pauseFile(info);
}

void FileTransfertWidget::paintEvent(QPaintEvent*)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
