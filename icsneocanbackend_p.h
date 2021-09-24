/****************************************************************************
** Copyright (C) 2021  Tomasz Ziobrowski <t.ziobrowski@3electrons.com>
****************************************************************************/

#ifndef ICSNEOCANBACKEND_P_H
#define ICSNEOCANBACKEND_P_H

#include "icsneocanbackend.h"
#include "icsneo/icsneocpp.h"
#include <memory>

#if defined(Q_OS_WIN32)
#  include <qt_windows.h>
#endif


QT_BEGIN_NAMESPACE

class QEvent;
class QSocketNotifier;
class QWinEventNotifier;
class QTimer;
class IcsNeoCanBackendPrivate;

namespace icsneo
{
  class Device;
  class CANMessage;
  class Message;
}



/**
 * @TODO
 * Verify channel distinguish when starting or multiplexing particular device
 * Verification of bitrate according to icsnoe enum types
 * Probably incoming event handler is not needed
 * Posibly outcoming either - this could simplify interface
 * Implement all configuration / parameters Keys with its proper support during opening device
 * Verify if device status could be better implemnted
 * implement resetDevice properly
 */

class IncomingEventHandler : public QObject
{
    // no Q_OBJECT macro!
public:
    explicit IncomingEventHandler(IcsNeoCanBackendPrivate *systecPrivate, QObject *parent) :
        QObject(parent),
        dptr(systecPrivate) { }

private:
       IcsNeoCanBackendPrivate * const dptr;
};


class IcsNeoCanBackendPrivate
{
    Q_DECLARE_PUBLIC(IcsNeoCanBackend)

public:
    IcsNeoCanBackendPrivate(IcsNeoCanBackend *q);

    bool setupDevice();
    bool open();
    void close();
    bool setConfigurationParameter(int key, const QVariant &value);
    bool setupChannel(const QString &interfaceName);
    void setupDefaultConfigurations();
    void enableWriteNotification(bool enable);
    void startWrite();
    void readAllReceivedMessages();

    void resetController();
    QCanBusDevice::CanBusStatus busStatus();

    static void interfaces( QList<QCanBusDeviceInfo> & list);

    void messageCallback(std::shared_ptr<icsneo::Message> m);
    QCanBusFrame interpretFrame( icsneo::CANMessage * msg );

    /*--------------*/
    IcsNeoCanBackend * const q_ptr;
    QTimer *outgoingEventNotifier = nullptr;
    IncomingEventHandler *incomingEventHandler = nullptr;

    quint8 device = 255;
    quint8 channel = 255;

    std::shared_ptr<icsneo::Device> m_device;
    static QMap<QString, std::shared_ptr<icsneo::Device>> m_devices;
    icsneo::Network m_network; //  = icsneo::Network::NetID::Invalid;

    int m_messageCallbackId = 0;
    bool m_hasFD = false;
    bool m_hasIso = false;
    bool m_hasTermination = false;
};

QT_END_NAMESPACE

#endif // ICSNEOCANBACKEND_P_H
