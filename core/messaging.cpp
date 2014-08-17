#include "messaging.h"

#include <QMutexLocker>
#include <tox/tox.h>

#define U8Ptr(x) reinterpret_cast<uint8_t*>(x)
#define CPtr(x) reinterpret_cast<const char*>(x)

CoreMessagingModule::CoreMessagingModule(QObject *parent, Tox *tox, QMutex *mutex)
    : CoreModule(parent, tox, mutex)
{
    // setup callbacks
    tox_callback_friend_message(tox, callbackFriendMessage, this);
}

void CoreMessagingModule::update()
{

}

void CoreMessagingModule::sendMessage(int friendnumber, QString msg)
{
    QMutexLocker lock(coreMutex());

    //TODO: split message after TOX_MAX_MESSAGE_LENGTH bytes
    tox_send_message(tox(), friendnumber, U8Ptr(msg.toUtf8().data()), msg.toUtf8().size());
}

void CoreMessagingModule::callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata)
{
    CoreMessagingModule* module = static_cast<CoreMessagingModule*>(userdata);
    QString msg = QString::fromUtf8(reinterpret_cast<const char*>(message), length);
    emit module->friendMessageReceived(friendnumber, msg);

    Q_UNUSED(tox)
}

