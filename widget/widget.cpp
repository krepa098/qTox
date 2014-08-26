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

#include "widget.h"
#include "ui_mainwindow.h"
#include "settings.h"
#include "friend.h"
#include "friendlist.h"
#include "widget/tool/friendrequestdialog.h"
#include "widget/friendwidget.h"
#include "grouplist.h"
#include "group.h"
#include "widget/groupwidget.h"
#include "widget/form/groupchatform.h"
#include "style.h"
#include <QMessageBox>
#include <QDebug>
#include <QSound>
#include <QTextStream>
#include <QFile>
#include <QString>
#include <QPainter>
#include <QMouseEvent>
#include <QDesktopWidget>
#include <QCursor>
#include <QSettings>
#include <QClipboard>

Widget *Widget::instance{nullptr};

Widget::Widget(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      activeFriendWidget{nullptr},
      activeGroupWidget{nullptr}
{
    ui->setupUi(this);

    ui->statusbar->hide();
    ui->menubar->hide();

    //restore window state
    restoreGeometry(Settings::getInstance().getWindowGeometry());
    restoreState(Settings::getInstance().getWindowState());
    ui->mainSplitter->restoreState(Settings::getInstance().getSplitterState());

    if (Settings::getInstance().getUseNativeDecoration())
    {
        ui->titleBar->hide();
        this->layout()->setContentsMargins(0, 0, 0, 0);

        ui->friendList->setObjectName("friendList");
        ui->friendList->setStyleSheet(Style::get(":ui/friendList/friendList.css"));
    }
    else
    {
        this->setObjectName("activeWindow");
        this->setStyleSheet(Style::get(":ui/window/window.css"));
        ui->statusPanel->setStyleSheet(QString(""));
        ui->friendList->setStyleSheet(QString(""));

        ui->friendList->setObjectName("friendList");
        ui->friendList->setStyleSheet(Style::get(":ui/friendList/friendList.css"));

        ui->tbMenu->setIcon(QIcon(":ui/window/applicationIcon.png"));
        ui->pbMin->setObjectName("minimizeButton");
        ui->pbMax->setObjectName("maximizeButton");
        ui->pbClose->setObjectName("closeButton");

        setWindowFlags(Qt::CustomizeWindowHint);
        setWindowFlags(Qt::FramelessWindowHint);

        addAction(ui->actionClose);

        connect(ui->pbMin, SIGNAL(clicked()), this, SLOT(minimizeBtnClicked()));
        connect(ui->pbMax, SIGNAL(clicked()), this, SLOT(maximizeBtnClicked()));
        connect(ui->pbClose, SIGNAL(clicked()), this, SLOT(close()));

        m_titleMode = FullTitle;
        moveWidget = false;
        inResizeZone = false;
        allowToResize = false;
        resizeVerSup = false;
        resizeHorEsq = false;
        resizeDiagSupEsq = false;
        resizeDiagSupDer = false;

        if (isMaximized())
        {
            showMaximized();
            ui->pbMax->setObjectName("restoreButton");
        }
    }

    isWindowMinimized = 0;

    ui->mainContent->setLayout(new QVBoxLayout());
    ui->mainHead->setLayout(new QVBoxLayout());
    ui->mainHead->layout()->setMargin(0);
    ui->mainHead->layout()->setSpacing(0);

    QWidget* friendListWidget = new QWidget();
    friendListWidget->setLayout(new QVBoxLayout());
    friendListWidget->layout()->setSpacing(0);
    friendListWidget->layout()->setMargin(0);
    friendListWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->friendList->setWidget(friendListWidget);
    ui->friendList->setLayoutDirection(Qt::RightToLeft);

    // delay setting username and message until Core inits
    //ui->nameLabel->setText(core->getUsername());
    ui->nameLabel->label->setStyleSheet("QLabel { color : white; font-size: 11pt; font-weight:bold;}");
    //ui->statusLabel->setText(core->getStatusMessage());
    ui->statusLabel->label->setStyleSheet("QLabel { color : white; font-size: 8pt;}");

    ui->statusButton->setStyleSheet(Style::get(":/ui/statusButton/statusButton.css"));

    QMenu *statusButtonMenu = new QMenu(ui->statusButton);
    QAction* setStatusOnline = statusButtonMenu->addAction(tr("Online","Button to set your status to 'Online'"));
    setStatusOnline->setIcon(QIcon(":ui/statusButton/dot_online.png"));
    QAction* setStatusAway = statusButtonMenu->addAction(tr("Away","Button to set your status to 'Away'"));
    setStatusAway->setIcon(QIcon(":ui/statusButton/dot_idle.png"));
    QAction* setStatusBusy = statusButtonMenu->addAction(tr("Busy","Button to set your status to 'Busy'"));
    setStatusBusy->setIcon(QIcon(":ui/statusButton/dot_busy.png"));
    ui->statusButton->setMenu(statusButtonMenu);

    ui->titleBar->setMouseTracking(true);
    ui->LTitle->setMouseTracking(true);
    ui->tbMenu->setMouseTracking(true);
    ui->pbMin->setMouseTracking(true);
    ui->pbMax->setMouseTracking(true);
    ui->pbClose->setMouseTracking(true);
    ui->statusHead->setMouseTracking(true);

    //ui->friendList->viewport()->installEventFilter(this);

    // disable proportional scaling
    ui->mainSplitter->setStretchFactor(0,0);
    ui->mainSplitter->setStretchFactor(1,1);

    ui->statusButton->setObjectName("offline");
    ui->statusButton->style()->polish(ui->statusButton);

    camera = new Camera;
    camview = new SelfCamView(camera);

    // create core in background thread
    coreThread = new QThread();
    core = new Core(Settings::getInstance().getEnableIPv6(), ToxProxy(), Settings::getInstance().getDhtServerList());
    core->loadConfig(Settings::getSettingsDirPath() + '/' + TOX_CONFIG_FILE_NAME);

    // connect core to core thread
    core->moveToThread(coreThread); // move this object all all of its children to the core thread
    connect(coreThread, &QThread::finished, core, &Core::deleteLater);
    connect(coreThread, &QThread::finished, coreThread, &QThread::deleteLater);
    connect(coreThread, &QThread::started, core, &Core::start); // this starts the core after we start the thread

    connect(core, &Core::connectionStatusChanged, this, &Widget::onConnectionStatusChanged);
    connect(core->msgModule(), &CoreMessengerModule::usernameChanged, this, &Widget::setUsername);
    connect(core->msgModule(), &CoreMessengerModule::statusChanged, this, &Widget::onStatusSet);
    connect(core->msgModule(), &CoreMessengerModule::userStatusMessageChanged, this, &Widget::setStatusMessage);
    connect(core->msgModule(), &CoreMessengerModule::friendAdded, this, &Widget::addFriend);
    connect(core->msgModule(), &CoreMessengerModule::friendStatusChanged, this, &Widget::onFriendStatusChanged);
    connect(core->msgModule(), &CoreMessengerModule::friendUsernameChanged, this, &Widget::onFriendUsernameChanged);
    connect(core->msgModule(), &CoreMessengerModule::friendStatusMessageChanged, this, &Widget::onFriendStatusMessageChanged);
    connect(core->msgModule(), &CoreMessengerModule::friendMessageReceived, this, &Widget::onFriendMessageReceived);

    // groups
    connect(core->msgModule(), &CoreMessengerModule::groupCreated, this, &Widget::onEmptyGroupCreated);
    connect(core->msgModule(), &CoreMessengerModule::groupInviteReceived, this, &Widget::onGroupInviteReceived);
    connect(core->msgModule(), &CoreMessengerModule::groupJoined, this, &Widget::onGroupJoined);
    connect(core->msgModule(), &CoreMessengerModule::groupMessage, this, &Widget::onGroupMessageReceived);
    connect(core->msgModule(), &CoreMessengerModule::groupInfoAvailable, this, &Widget::onGroupInfoAvailable);
    // group peers
    connect(core->msgModule(), &CoreMessengerModule::groupPeerJoined, this, &Widget::onGroupPeerJoined);
    connect(core->msgModule(), &CoreMessengerModule::groupPeerLeft, this, &Widget::onGroupPeerRemoved);
    connect(core->msgModule(), &CoreMessengerModule::groupPeerNameChanged, this, &Widget::onGroupPeerNameChanged);

    // friend requests
    connect(&friendForm, &AddFriendForm::friendRequested, core->msgModule(), &CoreMessengerModule::sendFriendRequest);
    connect(this, &Widget::friendRequestAccepted, core->msgModule(), &CoreMessengerModule::acceptFriendRequest);
    connect(core->msgModule(), &CoreMessengerModule::friendRequestReceived, this, &Widget::onFriendRequestReceived);

    // status changes
    connect(this, &Widget::statusSet, core->msgModule(), &CoreMessengerModule::setUserStatus);
    connect(this, &Widget::statusMessageChanged, core->msgModule(), &CoreMessengerModule::setUserStatusMessage);

    connect(&settingsForm.statusText, SIGNAL(editingFinished()), this, SLOT(onStatusMessageChanged()));

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(onAddClicked()));
    connect(ui->groupButton, SIGNAL(clicked()), this, SLOT(onGroupClicked()));
    connect(ui->transferButton, SIGNAL(clicked()), this, SLOT(onTransferClicked()));
    connect(ui->settingsButton, SIGNAL(clicked()), this, SLOT(onSettingsClicked()));
    connect(ui->nameLabel, &EditableLabelWidget::textChanged, this, &Widget::onUsernameChanged);
    //connect(&settingsForm.name, &QLineEdit::editingFinished, this, &Widget::onUsernameChanged);
    connect(ui->statusLabel, SIGNAL(textChanged(QString,QString)), this, SLOT(onStatusMessageChanged(QString,QString)));

    connect(setStatusOnline, SIGNAL(triggered()), this, SLOT(setStatusOnline()));
    connect(setStatusAway, SIGNAL(triggered()), this, SLOT(setStatusAway()));
    connect(setStatusBusy, SIGNAL(triggered()), this, SLOT(setStatusBusy()));

    friendForm.show(*ui);
    isFriendWidgetActive = 0;
    isGroupWidgetActive = 0;
}

Widget::~Widget()
{
    core->saveConfig(Settings::getSettingsDirPath() + '/' + TOX_CONFIG_FILE_NAME);
    instance = nullptr;
    coreThread->exit();
    coreThread->wait();
    delete camview;

    hideMainForms();

    for (Friend* f : FriendList::friendList)
        delete f;
    FriendList::friendList.clear();
    for (Group* g : GroupList::groupList)
        delete g;
    GroupList::groupList.clear();
    delete ui;
}

void Widget::postInit()
{
    //start core
    coreThread->start();
}

Widget* Widget::getInstance()
{
    if (!instance)
        instance = new Widget();
    return instance;
}

QThread* Widget::getCoreThread()
{
    return coreThread;
}

void Widget::closeEvent(QCloseEvent *event)
{
    Settings::getInstance().setWindowGeometry(saveGeometry());
    Settings::getInstance().setWindowState(saveState());
    Settings::getInstance().setSplitterState(ui->mainSplitter->saveState());
    QWidget::closeEvent(event);
}

QString Widget::getUsername()
{
    return ui->nameLabel->text();
}

Camera* Widget::getCamera()
{
    return camera;
}

void Widget::onConnected()
{
    emit statusSet(ToxStatus::Online);
}

void Widget::onDisconnected()
{
    emit statusSet(ToxStatus::Offline);
}

void Widget::onFailedToStartCore()
{
    QMessageBox critical(this);
    critical.setText("Toxcore failed to start, the application will terminate after you close this message.");
    critical.setIcon(QMessageBox::Critical);
    critical.exec();
    qApp->quit();
}

void Widget::onStatusSet(ToxStatus status)
{
    //We have to use stylesheets here, there's no way to
    //prevent the button icon from moving when pressed otherwise
    if (status == ToxStatus::Online)
    {
        ui->statusButton->setObjectName("online");
        ui->statusButton->style()->polish(ui->statusButton);
    }
    else if (status == ToxStatus::Away)
    {
        ui->statusButton->setObjectName("away");
        ui->statusButton->style()->polish(ui->statusButton);
    }
    else if (status == ToxStatus::Busy)
    {
        ui->statusButton->setObjectName("busy");
        ui->statusButton->style()->polish(ui->statusButton);
    }
    else if (status == ToxStatus::Offline)
    {
        ui->statusButton->setObjectName("offline");
        ui->statusButton->style()->polish(ui->statusButton);
    }
}

void Widget::onAddClicked()
{
    hideMainForms();
    friendForm.show(*ui);
}

void Widget::onGroupClicked()
{
    core->msgModule()->createGroup();
}

void Widget::onTransferClicked()
{
    hideMainForms();
    filesForm.show(*ui);
    isFriendWidgetActive = 0;
    isGroupWidgetActive = 0;
}

void Widget::onSettingsClicked()
{
    hideMainForms();
    settingsForm.show(*ui);
    isFriendWidgetActive = 0;
    isGroupWidgetActive = 0;
}

void Widget::hideMainForms()
{
    QLayoutItem* item;
    while ((item = ui->mainHead->layout()->takeAt(0)) != 0)
        item->widget()->hide();
    while ((item = ui->mainContent->layout()->takeAt(0)) != 0)
        item->widget()->hide();
    
    if (activeFriendWidget != nullptr)
    {
        Friend* f = FriendList::findFriend(activeFriendWidget->friendId);
        if (f != nullptr)
            activeFriendWidget->setAsInactiveChatroom();
    }
    if (activeGroupWidget != nullptr)
    {
        Group* g = GroupList::findGroup(activeGroupWidget->groupId);
        if (g != nullptr)
            activeGroupWidget->setAsInactiveChatroom();
    }
}

void Widget::onUsernameChanged(const QString& newUsername, const QString& oldUsername)
{
    ui->nameLabel->setText(oldUsername);
    ui->nameLabel->setToolTip(oldUsername);
    settingsForm.name.setText(oldUsername);

    core->msgModule()->setUsername(newUsername);
}

void Widget::setUsername(const QString& username)
{
    ui->nameLabel->setText(username);
    ui->nameLabel->setToolTip(username); // for overlength names
    settingsForm.name.setText(username);
}

void Widget::onStatusMessageChanged()
{
    const QString newStatusMessage = settingsForm.statusText.text();
    ui->statusLabel->setText(newStatusMessage);
    ui->statusLabel->setToolTip(newStatusMessage); // for overlength messsages
    settingsForm.statusText.setText(newStatusMessage);
    core->msgModule()->setUserStatusMessage(newStatusMessage);
}

void Widget::onStatusMessageChanged(const QString& newStatusMessage, const QString& oldStatusMessage)
{
    ui->statusLabel->setText(oldStatusMessage); // restore old status message until Core tells us to set it
    ui->statusLabel->setToolTip(oldStatusMessage); // for overlength messsages
    settingsForm.statusText.setText(oldStatusMessage);
    core->msgModule()->setUserStatusMessage(newStatusMessage);
}

void Widget::onConnectionStatusChanged(bool connected)
{
    onStatusSet(connected ? core->msgModule()->getUserStatus() : ToxStatus::Offline);
}

void Widget::setStatusMessage(const QString &statusMessage)
{
    ui->statusLabel->setText(statusMessage);
    ui->statusLabel->setToolTip(statusMessage); // for overlength messsages
    settingsForm.statusText.setText(statusMessage);
}

void Widget::addFriend(int friendId, const QString &userId)
{
    qDebug() << "Adding friend with id "+userId;
    Friend* newfriend = FriendList::addFriend(friendId, userId);
    QWidget* widget = ui->friendList->widget();
    QLayout* layout = widget->layout();
    layout->addWidget(newfriend->widget);

    connect(newfriend->widget, &FriendWidget::friendWidgetClicked, this, &Widget::onFriendWidgetClicked);
    connect(newfriend->widget, &FriendWidget::removeFriend, this, &Widget::removeFriend);
    connect(newfriend->widget, &FriendWidget::copyFriendIdToClipboard, this, &Widget::copyFriendIdToClipboard);
    connect(newfriend->chatForm, &ChatForm::sendMessage, core->msgModule(), &CoreMessengerModule::sendMessage);

    connect(newfriend->chatForm, &ChatForm::sendFile, core->ioModule(), &CoreIOModule::sendFile);

    //connect(core, &Core::fileTransferRequested, newfriend->chatForm, &ChatForm::onFileTransferRequest);

    //    connect(newfriend->chatForm, SIGNAL(sendFile(int32_t, QString, QString, long long)), core, SLOT(sendFile(int32_t, QString, QString, long long)));
    //    connect(newfriend->chatForm, SIGNAL(answerCall(int)), core, SLOT(answerCall(int)));
    //    connect(newfriend->chatForm, SIGNAL(hangupCall(int)), core, SLOT(hangupCall(int)));
    //    connect(newfriend->chatForm, SIGNAL(startCall(int)), core, SLOT(startCall(int)));
    //    connect(newfriend->chatForm, SIGNAL(startVideoCall(int,bool)), core, SLOT(startCall(int,bool)));
    //    connect(newfriend->chatForm, SIGNAL(cancelCall(int,int)), core, SLOT(cancelCall(int,int)));
    //    connect(core, &Core::fileReceiveRequested, newfriend->chatForm, &ChatForm::onFileRecvRequest);
    //    connect(core, &Core::avInvite, newfriend->chatForm, &ChatForm::onAvInvite);
    //    connect(core, &Core::avStart, newfriend->chatForm, &ChatForm::onAvStart);
    //    connect(core, &Core::avCancel, newfriend->chatForm, &ChatForm::onAvCancel);
    //    connect(core, &Core::avEnd, newfriend->chatForm, &ChatForm::onAvEnd);
    connect(core->avModule(), &CoreAVModule::callInviteRcv, newfriend->chatForm, &ChatForm::onAvInvite);
    connect(core->avModule(), &CoreAVModule::callStopped, newfriend->chatForm, &ChatForm::onAvCancel);
    connect(core->avModule(), &CoreAVModule::callStarted, newfriend->chatForm, &ChatForm::onAvStart);
    //    connect(core, &Core::avStarting, newfriend->chatForm, &ChatForm::onAvStarting);
    //    connect(core, &Core::avEnding, newfriend->chatForm, &ChatForm::onAvEnding);
    //    connect(core, &Core::avRequestTimeout, newfriend->chatForm, &ChatForm::onAvRequestTimeout);
    //    connect(core, &Core::avPeerTimeout, newfriend->chatForm, &ChatForm::onAvPeerTimeout);

    connect(newfriend->chatForm, &ChatForm::answerCall, core->avModule(), &CoreAVModule::answerCall);
    connect(newfriend->chatForm, &ChatForm::startCall, core->avModule(), &CoreAVModule::startCall);
    connect(newfriend->chatForm, &ChatForm::hangupCall, core->avModule(), &CoreAVModule::hangupCall);
}

void Widget::addFriendFailed(const QString&)
{
    QMessageBox::critical(0,"Error","Couldn't request friendship");
}

void Widget::onFriendStatusChanged(int friendId, ToxStatus status)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->friendStatus = status;
    updateFriendStatusLights(friendId);
}

void Widget::onFriendStatusMessageChanged(int friendId, const QString& message)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setStatusMessage(message);
}

void Widget::onFriendUsernameChanged(int friendId, const QString& username)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setName(username);
}

void Widget::onFriendStatusMessageLoaded(int friendId, const QString& message)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setStatusMessage(message);
}

void Widget::onFriendUsernameLoaded(int friendId, const QString& username)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setName(username);
}

void Widget::onFriendWidgetClicked(FriendWidget *widget)
{
    Friend* f = FriendList::findFriend(widget->friendId);
    if (!f)
        return;

    hideMainForms();
    f->chatForm->show(*ui);
    if (activeFriendWidget != nullptr)
    {
        activeFriendWidget->setAsInactiveChatroom();
    }
    activeFriendWidget = widget;
    widget->setAsActiveChatroom();
    isFriendWidgetActive = 1;
    isGroupWidgetActive = 0;

    if (f->hasNewMessages != 0)
        f->hasNewMessages = 0;

    updateFriendStatusLights(f->friendId);
}

void Widget::onFriendMessageReceived(int friendId, const QString& message)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->chatForm->addFriendMessage(message);

    if (activeFriendWidget != nullptr)
    {
        Friend* f2 = FriendList::findFriend(activeFriendWidget->friendId);
        if (((f->friendId != f2->friendId) || isFriendWidgetActive == 0) || isWindowMinimized || !isActiveWindow())
        {
            f->hasNewMessages = 1;
            newMessageAlert();
        }
    }
    else
    {
        f->hasNewMessages = 1;
        newMessageAlert();
    }

    updateFriendStatusLights(friendId);
}

void Widget::updateFriendStatusLights(int friendId)
{
    Friend* f = FriendList::findFriend(friendId);
    ToxStatus status = f->friendStatus;
    if (status == ToxStatus::Online && f->hasNewMessages == 0)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_online.png"));
    else if (status == ToxStatus::Online && f->hasNewMessages == 1)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_online_notification.png"));
    else if (status == ToxStatus::Away && f->hasNewMessages == 0)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_idle.png"));
    else if (status == ToxStatus::Away && f->hasNewMessages == 1)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_idle_notification.png"));
    else if (status == ToxStatus::Busy && f->hasNewMessages == 0)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_busy.png"));
    else if (status == ToxStatus::Busy && f->hasNewMessages == 1)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_busy_notification.png"));
    else if (status == ToxStatus::Offline && f->hasNewMessages == 0)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_away.png"));
    else if (status == ToxStatus::Offline && f->hasNewMessages == 1)
        f->widget->statusPic.setPixmap(QPixmap(":img/status/dot_away_notification.png"));
}

void Widget::newMessageAlert()
{
    QApplication::alert(this);
    QSound::play(":audio/notification.wav");
}

void Widget::onFriendRequestReceived(const ToxPublicKey& userId, const QString& message)
{
    FriendRequestDialog dialog(this, userId.toHex().toUpper(), message);

    if (dialog.exec() == QDialog::Accepted)
        emit friendRequestAccepted(userId);
}

void Widget::removeFriend(int friendId)
{
    Friend* f = FriendList::findFriend(friendId);
    f->widget->setAsInactiveChatroom();
    if (f->widget == activeFriendWidget)
        activeFriendWidget = nullptr;
    FriendList::removeFriend(friendId);
    core->msgModule()->removeFriend(friendId);
    delete f;
    if (ui->mainHead->layout()->isEmpty())
        onAddClicked();
}

void Widget::copyFriendIdToClipboard(int friendId)
{
    Friend* f = FriendList::findFriend(friendId);
    if (f != nullptr)
    {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(f->userId, QClipboard::Clipboard);
    }
}

void Widget::onGroupInviteReceived(int friendId, ToxPublicKey groupPublicKey)
{
    core->msgModule()->acceptGroupInvite(friendId, groupPublicKey);
}

void Widget::onGroupPeerJoined(int groupnumber, int peer, QString name)
{
//    Group* g = GroupList::findGroup(groupnumber);
//    if (g)
//        g->addPeer(peer, name);
}

void Widget::onGroupPeerRemoved(int groupnumber, int peer, QString name)
{
//    Group* g = GroupList::findGroup(groupnumber);
//    if (g)
//        g->removePeer(peer);
}

void Widget::onGroupPeerNameChanged(int groupnumber, int peer, QString name)
{
//    Group* g = GroupList::findGroup(groupnumber);
//    if (g)
//        g->updatePeer(peer, name);
}

void Widget::onGroupMessageReceived(int groupnumber, int friendgroupnumber, const QString& message)
{
    Group* g = GroupList::findGroup(groupnumber);
    if (!g)
        return;

    g->chatForm->addGroupMessage(message, friendgroupnumber);

    if ((isGroupWidgetActive != 1 || (activeGroupWidget && g->groupId != activeGroupWidget->groupId)) || isWindowMinimized || !isActiveWindow())
    {
        if (message.contains(core->msgModule()->getUsername(), Qt::CaseInsensitive))
        {
            newMessageAlert();
            g->hasNewMessages = 1;
            g->userWasMentioned = 1;
            if (Settings::getInstance().getUseNativeDecoration())
                g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_online_notification.png"));
            else
                g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_groupchat_notification.png"));
        }
        else
            if (g->hasNewMessages == 0)
            {
                g->hasNewMessages = 1;
                if (Settings::getInstance().getUseNativeDecoration())
                    g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_online_notification.png"));
                else
                    g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_groupchat_newmessages.png"));
            }
    }
}

void Widget::onGroupInfoAvailable(ToxGroupInfo info)
{
    Group* g = GroupList::findGroup(info.number);
    if (g)
        g->updatePeers(info.peers);
}

void Widget::onGroupWidgetClicked(GroupWidget* widget)
{
    Group* g = GroupList::findGroup(widget->groupId);
    if (!g)
        return;

    hideMainForms();
    g->chatForm->show(*ui);
    if (activeGroupWidget != nullptr)
    {
        activeGroupWidget->setAsInactiveChatroom();
    }
    activeGroupWidget = widget;
    widget->setAsActiveChatroom();
    isFriendWidgetActive = 0;
    isGroupWidgetActive = 1;

    if (g->hasNewMessages != 0)
    {
        g->hasNewMessages = 0;
        g->userWasMentioned = 0;
        if (Settings::getInstance().getUseNativeDecoration())
            g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_online.png"));
        else
            g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_groupchat.png"));
    }
}

void Widget::onGroupJoined(int groupnumber)
{
    createGroup(groupnumber);
}

void Widget::removeGroup(int groupId)
{
    Group* g = GroupList::findGroup(groupId);
    g->widget->setAsInactiveChatroom();
    if (g->widget == activeGroupWidget)
        activeGroupWidget = nullptr;
    GroupList::removeGroup(groupId);
    core->msgModule()->removeGroup(groupId);
    delete g;
    if (ui->mainHead->layout()->isEmpty())
        onAddClicked();
}

Core *Widget::getCore()
{
    return core;
}

Group *Widget::createGroup(int groupId)
{
    qDebug() << "Create group" << groupId;
    Group* g = GroupList::findGroup(groupId);
    if (g)
    {
        qWarning() << "Widget::createGroup: Group already exists";
        return g;
    }

    QString groupName = QString("Groupchat #%1").arg(groupId);
    Group* newgroup = GroupList::addGroup(groupId, groupName);
    QWidget* widget = ui->friendList->widget();
    QLayout* layout = widget->layout();
    layout->addWidget(newgroup->widget);
    if (!Settings::getInstance().getUseNativeDecoration())
        newgroup->widget->statusPic.setPixmap(QPixmap(":img/status/dot_groupchat.png"));

    connect(newgroup->widget, SIGNAL(groupWidgetClicked(GroupWidget*)), this, SLOT(onGroupWidgetClicked(GroupWidget*)));
    connect(newgroup->widget, SIGNAL(removeGroup(int)), this, SLOT(removeGroup(int)));

    connect(newgroup->chatForm, &GroupChatForm::sendMessage, core->msgModule(), &CoreMessengerModule::sendGroupMessage);

    return newgroup;
}

void Widget::showTestCamview()
{
    camview->show();
}

void Widget::onEmptyGroupCreated(int groupId)
{
    createGroup(groupId);
}

bool Widget::isFriendWidgetCurActiveWidget(Friend* f)
{
    if (!f)
        return false;
    if (activeFriendWidget != nullptr)
    {
        Friend* f2 = FriendList::findFriend(activeFriendWidget->friendId);
        if ((f->friendId != f2->friendId) || isFriendWidgetActive == 0)
            return false;
    }
    else
        return false;
    return true;
}

bool Widget::event(QEvent * e)
{

    if( e->type() == QEvent::WindowStateChange )
    {
        if(windowState().testFlag(Qt::WindowMinimized) == true)
        {
            isWindowMinimized = 1;
        }
    }
    else if (e->type() == QEvent::WindowActivate)
    {
        if (!Settings::getInstance().getUseNativeDecoration())
        {
            this->setObjectName("activeWindow");
            this->style()->polish(this);
        }
        isWindowMinimized = 0;
        if (isFriendWidgetActive && activeFriendWidget != nullptr)
        {
            Friend* f = FriendList::findFriend(activeFriendWidget->friendId);
            f->hasNewMessages = 0;
            updateFriendStatusLights(f->friendId);
        }
        else if (isGroupWidgetActive && activeGroupWidget != nullptr)
        {
            Group* g = GroupList::findGroup(activeGroupWidget->groupId);
            g->hasNewMessages = 0;
            g->userWasMentioned = 0;
            if (Settings::getInstance().getUseNativeDecoration())
                g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_online.png"));
            else
                g->widget->statusPic.setPixmap(QPixmap(":img/status/dot_groupchat.png"));
        }
    }
    else if (e->type() == QEvent::WindowDeactivate && !Settings::getInstance().getUseNativeDecoration())
    {
        this->setObjectName("inactiveWindow");
        this->style()->polish(this);
    }
    else if (e->type() == QEvent::MouseMove && !Settings::getInstance().getUseNativeDecoration())
    {
        QMouseEvent *k = (QMouseEvent *)e;
        int xMouse = k->pos().x();
        int yMouse = k->pos().y();
        int wWidth = this->geometry().width();
        int wHeight = this->geometry().height();

        if (moveWidget)
        {
            inResizeZone = false;
            moveWindow(k);
        }
        else if (allowToResize)
            resizeWindow(k);
        else if (xMouse >= wWidth - PIXELS_TO_ACT or allowToResize)
        {
            inResizeZone = true;

            if (yMouse >= wHeight - PIXELS_TO_ACT)
            {
                setCursor(Qt::SizeFDiagCursor);
                resizeWindow(k);
            }
            else if (yMouse <= PIXELS_TO_ACT)
            {
                setCursor(Qt::SizeBDiagCursor);
                resizeWindow(k);
            }

        }
        else
        {
            inResizeZone = false;
            setCursor(Qt::ArrowCursor);
        }

        e->accept();
    }

    return QWidget::event(e);
}

void Widget::mousePressEvent(QMouseEvent *e)
{
    if (!Settings::getInstance().getUseNativeDecoration())
    {
        if (e->button() == Qt::LeftButton)
        {
            if (inResizeZone)
            {
                allowToResize = true;

                if (e->pos().y() <= PIXELS_TO_ACT)
                {
                    if (e->pos().x() <= PIXELS_TO_ACT)
                        resizeDiagSupEsq = true;
                    else if (e->pos().x() >= geometry().width() - PIXELS_TO_ACT)
                        resizeDiagSupDer = true;
                    else
                        resizeVerSup = true;
                }
                else if (e->pos().x() <= PIXELS_TO_ACT)
                    resizeHorEsq = true;
            }
            else if (e->pos().x() >= PIXELS_TO_ACT and e->pos().x() < ui->titleBar->geometry().width()
                     and e->pos().y() >= PIXELS_TO_ACT and e->pos().y() < ui->titleBar->geometry().height())
            {
                moveWidget = true;
                dragPosition = e->globalPos() - frameGeometry().topLeft();
            }
        }

        e->accept();
    }
}

void Widget::mouseReleaseEvent(QMouseEvent *e)
{
    if (!Settings::getInstance().getUseNativeDecoration())
    {
        moveWidget = false;
        allowToResize = false;
        resizeVerSup = false;
        resizeHorEsq = false;
        resizeDiagSupEsq = false;
        resizeDiagSupDer = false;

        e->accept();
    }
}

void Widget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (!Settings::getInstance().getUseNativeDecoration())
    {
        if (e->pos().x() < ui->tbMenu->geometry().right() and e->pos().y() < ui->tbMenu->geometry().bottom()
                and e->pos().x() >=  ui->tbMenu->geometry().x() and e->pos().y() >= ui->tbMenu->geometry().y()
                and ui->tbMenu->isVisible())
            close();
        else if (e->pos().x() < ui->titleBar->geometry().width()
                 and e->pos().y() < ui->titleBar->geometry().height()
                 and m_titleMode != FullScreenMode)
            maximizeBtnClicked();
        e->accept();
    }
}

void Widget::paintEvent (QPaintEvent *)
{
    QStyleOption opt;
    opt.init (this);
    QPainter p(this);
    style()->drawPrimitive (QStyle::PE_Widget, &opt, &p, this);
}

void Widget::moveWindow(QMouseEvent *e)
{
    if (!Settings::getInstance().getUseNativeDecoration())
    {
        if (e->buttons() & Qt::LeftButton)
        {
            move(e->globalPos() - dragPosition);
            e->accept();
        }
    }
}

void Widget::resizeWindow(QMouseEvent *e)
{
    if (!Settings::getInstance().getUseNativeDecoration())
    {
        if (allowToResize)
        {
            int xMouse = e->pos().x();
            int yMouse = e->pos().y();
            int wWidth = geometry().width();
            int wHeight = geometry().height();

            if (cursor().shape() == Qt::SizeVerCursor)
            {
                if (resizeVerSup)
                {
                    int newY = geometry().y() + yMouse;
                    int newHeight = wHeight - yMouse;

                    if (newHeight > minimumSizeHint().height())
                    {
                        resize(wWidth, newHeight);
                        move(geometry().x(), newY);
                    }
                }
                else
                    resize(wWidth, yMouse+1);
            }
            else if (cursor().shape() == Qt::SizeHorCursor)
            {
                if (resizeHorEsq)
                {
                    int newX = geometry().x() + xMouse;
                    int newWidth = wWidth - xMouse;

                    if (newWidth > minimumSizeHint().width())
                    {
                        resize(newWidth, wHeight);
                        move(newX, geometry().y());
                    }
                }
                else
                    resize(xMouse, wHeight);
            }
            else if (cursor().shape() == Qt::SizeBDiagCursor)
            {
                int newX = 0;
                int newWidth = 0;
                int newY = 0;
                int newHeight = 0;

                if (resizeDiagSupDer)
                {
                    newX = geometry().x();
                    newWidth = xMouse;
                    newY = geometry().y() + yMouse;
                    newHeight = wHeight - yMouse;
                }
                else
                {
                    newX = geometry().x() + xMouse;
                    newWidth = wWidth - xMouse;
                    newY = geometry().y();
                    newHeight = yMouse;
                }

                if (newWidth >= minimumSizeHint().width() and newHeight >= minimumSizeHint().height())
                {
                    resize(newWidth, newHeight);
                    move(newX, newY);
                }
                else if (newWidth >= minimumSizeHint().width())
                {
                    resize(newWidth, wHeight);
                    move(newX, geometry().y());
                }
                else if (newHeight >= minimumSizeHint().height())
                {
                    resize(wWidth, newHeight);
                    move(geometry().x(), newY);
                }
            }
            else if (cursor().shape() == Qt::SizeFDiagCursor)
            {
                if (resizeDiagSupEsq)
                {
                    int newX = geometry().x() + xMouse;
                    int newWidth = wWidth - xMouse;
                    int newY = geometry().y() + yMouse;
                    int newHeight = wHeight - yMouse;

                    if (newWidth >= minimumSizeHint().width() and newHeight >= minimumSizeHint().height())
                    {
                        resize(newWidth, newHeight);
                        move(newX, newY);
                    }
                    else if (newWidth >= minimumSizeHint().width())
                    {
                        resize(newWidth, wHeight);
                        move(newX, geometry().y());
                    }
                    else if (newHeight >= minimumSizeHint().height())
                    {
                        resize(wWidth, newHeight);
                        move(geometry().x(), newY);
                    }
                }
                else
                    resize(xMouse+1, yMouse+1);
            }

            e->accept();
        }
    }
}

void Widget::setCentralWidget(QWidget *widget, const QString &widgetName)
{
    connect(widget, SIGNAL(cancelled()), this, SLOT(close()));

    centralLayout->addWidget(widget);
    //ui->centralWidget->setLayout(centralLayout);
    ui->LTitle->setText(widgetName);
}

void Widget::setTitlebarMode(const TitleMode &flag)
{
    m_titleMode = flag;

    switch (m_titleMode)
    {
    case CleanTitle:
        ui->tbMenu->setHidden(true);
        ui->pbMin->setHidden(true);
        ui->pbMax->setHidden(true);
        ui->pbClose->setHidden(true);
        break;
    case OnlyCloseButton:
        ui->tbMenu->setHidden(true);
        ui->pbMin->setHidden(true);
        ui->pbMax->setHidden(true);
        break;
    case MenuOff:
        ui->tbMenu->setHidden(true);
        break;
    case MaxMinOff:
        ui->pbMin->setHidden(true);
        ui->pbMax->setHidden(true);
        break;
    case FullScreenMode:
        ui->pbMax->setHidden(true);
        showMaximized();
        break;
    case MaximizeModeOff:
        ui->pbMax->setHidden(true);
        break;
    case MinimizeModeOff:
        ui->pbMin->setHidden(true);
        break;
    case FullTitle:
        ui->tbMenu->setVisible(true);
        ui->pbMin->setVisible(true);
        ui->pbMax->setVisible(true);
        ui->pbClose->setVisible(true);
        break;
        break;
    default:
        ui->tbMenu->setVisible(true);
        ui->pbMin->setVisible(true);
        ui->pbMax->setVisible(true);
        ui->pbClose->setVisible(true);
        break;
    }
    ui->LTitle->setVisible(true);
}

void Widget::setTitlebarMenu(QMenu *menu, const QString &icon)
{
    ui->tbMenu->setMenu(menu);
    ui->tbMenu->setIcon(QIcon(icon));
}

void Widget::maximizeBtnClicked()
{
    if (isFullScreen() or isMaximized())
    {
        ui->pbMax->setIcon(QIcon(":/ui/images/app_max.png"));
        setWindowState(windowState() & ~Qt::WindowFullScreen & ~Qt::WindowMaximized);
    }
    else
    {
        ui->pbMax->setIcon(QIcon(":/ui/images/app_rest.png"));
        setWindowState(windowState() | Qt::WindowFullScreen | Qt::WindowMaximized);
    }
}

void Widget::minimizeBtnClicked()
{
    if (isMinimized())
    {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    else
    {
        setWindowState(windowState() | Qt::WindowMinimized);
    }
}

void Widget::setStatusOnline()
{
    core->msgModule()->setUserStatus(ToxStatus::Online);
}

void Widget::setStatusAway()
{
    core->msgModule()->setUserStatus(ToxStatus::Away);
}

void Widget::setStatusBusy()
{
    core->msgModule()->setUserStatus(ToxStatus::Busy);
}

bool Widget::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::Wheel)
    {
        QWheelEvent * whlEvnt =  static_cast< QWheelEvent * >( event );
        whlEvnt->angleDelta().setX(0);
    }
    return false;
}
