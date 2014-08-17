#ifndef MODULE_H
#define MODULE_H

#include <QObject>
#include <QMutex>

struct Tox;

class CoreModule : public QObject
{
    Q_OBJECT
public:
    CoreModule(QObject* parent, Tox* tox, QMutex* mutex)
        : QObject(parent),
          m_tox(tox),
          m_coreMutex(mutex)
    {
    }

    Tox* tox() {
        return m_tox;
    }

    QMutex* coreMutex()
    {
        return m_coreMutex;
    }

    virtual void update() = 0;

private:
    Tox* m_tox;
    QMutex* m_coreMutex;
};

#endif // MODULE_H
