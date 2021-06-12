/****************************************************************************
** Copyright (C) 2021  Tomasz Ziobrowski <t.ziobrowski@3electrons.com>
****************************************************************************/


#include "icsneocanbackend.h"
#include "icsneocanbackend_p.h"
#include "icsneo/icsneocpp.h"

#include <QtSerialBus/qcanbusdevice.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qtimer.h>

QT_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(QT_CANBUS_PLUGINS_ICSNEOCAN)

class OutgoingEventNotifier : public QTimer
{
public:
    OutgoingEventNotifier(IcsNeoCanBackendPrivate *d, QObject *parent) :
        QTimer(parent),
        dptr(d)
    {
    }
protected:
    void timerEvent(QTimerEvent *e) override
    {
        if (e->timerId() == timerId()) {
            dptr->startWrite();
            return;
        }
        QTimer::timerEvent(e);
    }
private:
    IcsNeoCanBackendPrivate * const dptr;
};


/*-----------------------------------------------------------------------------------------
                             B A C K E N D   P R I V A T E
-----------------------------------------------------------------------------------------*/


//std::vector<std::shared_ptr<icsneo::Device>>  IcsNeoCanBackendPrivate::m_devices;

IcsNeoCanBackendPrivate::IcsNeoCanBackendPrivate(IcsNeoCanBackend *q) :
    q_ptr(q),
    incomingEventHandler(new IncomingEventHandler(this, q))
{
}

bool IcsNeoCanBackendPrivate::open()
{
    Q_Q(IcsNeoCanBackend);

    if (!m_device.get()) // device does not exist anymore
        return false;

    const int bitrate     = q->configurationParameter(QCanBusDevice::BitRateKey).toInt();

    /*  @TODO - implement remainging keys
    const bool receiveOwn = q->configurationParameter(QCanBusDevice::ReceiveOwnKey).toBool();
    const bool loopback   = q->configurationParameter(QCanBusDevice::LoopbackKey).toBool();

    RawFilterKey = 0,
    ErrorFilterKey,
    LoopbackKey,
    ReceiveOwnKey,
    CanFdKey,
    DataBitRateKey,
    ProtocolKey,
    UserKey = 30
    */

    bool res =  m_device->open();

    res &= m_device->settings->setBaudrateFor(icsneo::Network::NetID::HSCAN, bitrate);
    res &= m_device->settings->apply();
    res &= m_device->goOnline();

    if (!res)
         q->setError(QString::fromStdString(icsneo::GetLastError().describe()),
                     QCanBusDevice::ConnectionError);
    else
    {
        m_messageCallbackId = m_device->addMessageCallback(icsneo::MessageCallback([=](std::shared_ptr<icsneo::Message> m)
        {
            QVector<QCanBusFrame> newFrames;
            auto msg = std::static_pointer_cast<icsneo::CANMessage>(m);
            QCanBusFrame frame = interpretFrame(msg.get());
            if (frame.isValid())
            {
               newFrames.append(frame);
                q->enqueueReceivedFrames(newFrames);
            }
        } ));
    }
    return res;
}

void IcsNeoCanBackendPrivate::close()
{
    Q_Q(IcsNeoCanBackend);

    enableWriteNotification(false);

    if (outgoingEventNotifier) {
        delete outgoingEventNotifier;
        outgoingEventNotifier = nullptr;
   }

   if (m_messageCallbackId)
       m_device->removeMessageCallback(m_messageCallbackId);

   bool res = m_device->goOffline() && m_device->close();

    if (!res)
        q->setError(QString::fromStdString(icsneo::GetLastError().describe()),
                    QCanBusDevice::OperationError);

}

void IcsNeoCanBackendPrivate::eventHandler(QEvent *event)
{
    const int code = event->type() - QEvent::User;

    if (code == 1)
        readAllReceivedMessages();
}

bool IcsNeoCanBackendPrivate::setConfigurationParameter(int key, const QVariant &value)
{
    Q_Q(IcsNeoCanBackend);

    switch (key) {
    case QCanBusDevice::BitRateKey:
        return verifyBitRate(value.toInt());
    case QCanBusDevice::ReceiveOwnKey:
        if (Q_UNLIKELY(q->state() != QCanBusDevice::UnconnectedState)) {
            q->setError(IcsNeoCanBackend::tr("Cannot configure TxEcho for open device"),
                        QCanBusDevice::ConfigurationError);
            return false;
        }
        return true;
    default:
        q->setError(IcsNeoCanBackend::tr("Unsupported configuration key: %1").arg(key),
                    QCanBusDevice::ConfigurationError);
        return false;
    }
}

bool IcsNeoCanBackendPrivate::setupChannel(const QString &interfaceName)
{
    Q_Q(IcsNeoCanBackend);

    const QRegularExpression re(QStringLiteral("can(\\d)\\.(\\d)"));
    const QRegularExpressionMatch match = re.match(interfaceName);

    if (Q_LIKELY(match.hasMatch())) {
        device = quint8(match.captured(1).toUShort());
        channel = quint8(match.captured(2).toUShort());
    } else {
        q->setError(IcsNeoCanBackend::tr("Invalid interface '%1'.")
                    .arg(interfaceName), QCanBusDevice::ConnectionError);
        return false;
    }

    return true;
}

void IcsNeoCanBackendPrivate::setupDefaultConfigurations()
{
    Q_Q(IcsNeoCanBackend);
    q->setConfigurationParameter(QCanBusDevice::BitRateKey, 500000);
    q->setConfigurationParameter(QCanBusDevice::ReceiveOwnKey, false);
    q->setConfigurationParameter(QCanBusDevice::CanFdKey, false);
}

void IcsNeoCanBackendPrivate::enableWriteNotification(bool enable)
{
    Q_Q(IcsNeoCanBackend);

    if (outgoingEventNotifier) {
        if (enable) {
            if (!outgoingEventNotifier->isActive())
                outgoingEventNotifier->start();
        } else {
            outgoingEventNotifier->stop();
        }
    } else if (enable) {
        outgoingEventNotifier = new OutgoingEventNotifier(this, q);
        outgoingEventNotifier->start(0);
    }
}

void IcsNeoCanBackendPrivate::startWrite()
{
    Q_Q(IcsNeoCanBackend);

    if (!q->hasOutgoingFrames()) {
        enableWriteNotification(false);
        return;
    }

    const QCanBusFrame frame = q->dequeueOutgoingFrame();
    const QByteArray payload = frame.payload();

    auto msg = std::make_shared<icsneo::CANMessage>();
    msg->network = icsneo::Network::NetID::HSCAN;
    msg->data.insert(msg->data.end(), payload.begin(), payload.end()) ;
    msg->arbid =frame.frameId();
    msg->isRemote = frame.frameType() == QCanBusFrame::RemoteRequestFrame;
    msg->isExtended = frame.hasExtendedFrameFormat();
    msg->isCANFD =  q->configurationParameter(QCanBusDevice::CanFdKey).toBool();
    msg->baudrateSwitch = frame.hasBitrateSwitch();
    msg->errorStateIndicator = frame.hasErrorStateIndicator();

    // Attempt to transmit the sample msg
     if(m_device->transmit(msg))
       q->framesWritten(qint64(1));
     else
           q->setError(QString::fromStdString(icsneo::GetLastError().describe()),
                       QCanBusDevice::WriteError) ;

    if (q->hasOutgoingFrames())
        enableWriteNotification(true);
}

void IcsNeoCanBackendPrivate::readAllReceivedMessages()
{
    Q_Q(IcsNeoCanBackend);
    QVector<QCanBusFrame> newFrames;
    std::vector<std::shared_ptr<icsneo::Message>> msgs;
    // Attempt to get messages, limiting the number of messages at once to 50,000
    // A third parameter of type std::chrono::milliseconds is also accepted if a timeout is desired
    if(!m_device->getMessages(msgs, 50000, std::chrono::milliseconds(500)))
    {
       q->setError(QString::fromStdString(icsneo::GetLastError().describe()),
                   QCanBusDevice::ReadError) ;
                   return;
    }

    for (auto m: msgs)
    {
        auto msg = std::static_pointer_cast<icsneo::CANMessage>(m);
        QCanBusFrame frame = interpretFrame(msg.get());
        if (frame.isValid())
          newFrames.append(std::move(frame));
    }
    q->enqueueReceivedFrames(newFrames);
}

bool IcsNeoCanBackendPrivate::verifyBitRate(int bitrate)
{
    Q_Q(IcsNeoCanBackend);

    if (Q_UNLIKELY(q->state() != QCanBusDevice::UnconnectedState)) {
        q->setError(IcsNeoCanBackend::tr("Cannot configure bitrate for open device"),
                    QCanBusDevice::ConfigurationError);
        return false;
    }
    /*
    if (Q_UNLIKELY(bitrateCodeFromBitrate(bitrate) == 0)) {
        q->setError(IcsNeoCanBackend::tr("Unsupported bitrate %1.").arg(bitrate),
                    QCanBusDevice::ConfigurationError);
        return false;
    }
    */
    return true;
}

void IcsNeoCanBackendPrivate::resetController()
{
    //@TODO: Implement me
    qDebug("IcsNeoCanBackendPrivate::resetController() - non implemented");
}

QCanBusFrame IcsNeoCanBackendPrivate::interpretFrame( icsneo::CANMessage * msg )
{
    if (icsneo::Network::Type::CAN != msg->network.getType())
        return QCanBusFrame(QCanBusFrame::InvalidFrame);

    QByteArray data;
    for (auto b : msg->data)
        data.append(b);

    QCanBusFrame frame(msg->arbid, data);

    frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(msg->timestamp));
    frame.setExtendedFrameFormat(msg->isExtended);
    //@TODO - how to set get info about localecho ?
    //frame.setLocalEcho(msg->transmited);
    frame.setErrorStateIndicator(msg->errorStateIndicator);
    if (msg->error)
        frame.setFrameType(QCanBusFrame::ErrorFrame);
    else if (msg->isRemote)
         frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
    else
         frame.setFrameType(QCanBusFrame::DataFrame);
    return frame;
}

QCanBusDevice::CanBusStatus IcsNeoCanBackendPrivate::busStatus()
{
    Q_Q(IcsNeoCanBackend);

    if (icsneo::GetLastError().getSeverity() == icsneo::APIEvent::Severity::EventWarning )
         return QCanBusDevice::CanBusStatus::Warning;

    if (icsneo::GetLastError().getSeverity() == icsneo::APIEvent::Severity::Error )
    {
        q->setError(QString::fromStdString(icsneo::GetLastError().describe()), QCanBusDevice::ConfigurationError);
        return QCanBusDevice::CanBusStatus::Error;
    }

    if (m_device->isOnline())
       return QCanBusDevice::CanBusStatus::Good;
    else
        return QCanBusDevice::CanBusStatus::BusOff;

    return QCanBusDevice::CanBusStatus::Unknown;
}



QMap<QString, std::shared_ptr<icsneo::Device>> IcsNeoCanBackendPrivate::m_devices;

void IcsNeoCanBackendPrivate::interfaces( QList<QCanBusDeviceInfo> & list)
{
    std::vector<std::shared_ptr<icsneo::Device>>  devices = icsneo::FindAllDevices();
    int channel = 0;
    m_devices.clear();

    for (int i = 0 ; i < devices.size() ; i++)
    {
      // for QCanBusDeviceInfo
      std::shared_ptr<icsneo::Device> & device = devices[i];
      QString description =   QString::fromStdString(device->describe());
      QString serial      = QString::fromStdString(device->getSerial());

      QCanBusDeviceInfo info = IcsNeoCanBackend::createDeviceInfo(serial, description, i, channel);
      m_devices[info.name()] = device;
      list.append(std::move(info));
      device.reset();
    }
}


/*-----------------------------------------------------------------------------------------
                                        B A C K E N D
-----------------------------------------------------------------------------------------*/
IcsNeoCanBackend::IcsNeoCanBackend(const QString &name, QObject *parent) :
    QCanBusDevice(parent),
    d_ptr(new IcsNeoCanBackendPrivate(this))
{
    Q_D(IcsNeoCanBackend);

    d->setupChannel(name);
    d->setupDefaultConfigurations();
    std::function<void()> f = std::bind(&IcsNeoCanBackend::resetController, this);
    setResetControllerFunction(f);
    std::function<CanBusStatus()> g = std::bind(&IcsNeoCanBackend::busStatus, this);
    setCanBusStatusGetter(g);
    QList<QCanBusDeviceInfo> list;
    d_ptr->m_device = IcsNeoCanBackendPrivate::m_devices[name];
}

IcsNeoCanBackend::~IcsNeoCanBackend()
{
    if (state() == QCanBusDevice::ConnectedState)
        close();
    d_ptr->m_device.reset(); // static table is for deleting it
    delete d_ptr;
}

QCanBusDeviceInfo IcsNeoCanBackend::createDeviceInfo(const QString &serialNumber,
                                                     const QString &description,
                                                     uint deviceNumber,
                                                     int channelNumber)
{
    const QString name = QString::fromLatin1("can%1.%2").arg(deviceNumber).arg(channelNumber);
    qDebug() <<" Device name:serial:description" << name << ":" << serialNumber << ":" << name ;
    return QCanBusDevice::createDeviceInfo(name, serialNumber, description, channelNumber, false, true);
}

QList<QCanBusDeviceInfo> IcsNeoCanBackend::interfaces()
{
   QList<QCanBusDeviceInfo> list;
   IcsNeoCanBackendPrivate::interfaces(list);
   return list;
}

QString IcsNeoCanBackend::interpretErrorFrame(const QCanBusFrame &errorFrame)
{
    // TODO: Implement me
    Q_UNUSED(errorFrame);
    return QString();
}

bool IcsNeoCanBackend::open()
{
    Q_D(IcsNeoCanBackend);
    if (!d->open())
        return false;

    // Apply all stored configurations except bitrate and receive own,
    // because these cannot be applied after opening the device
    const QVector<int> keys = configurationKeys();
    for (int key : keys) {
        if (key == BitRateKey || key == ReceiveOwnKey)
            continue;
        const QVariant param = configurationParameter(key);
        const bool success = d->setConfigurationParameter(key, param);
#ifndef _WIN32  // @TODO - make it compile with MinGw
        if (Q_UNLIKELY(!success)) {
            qCWarning(QT_CANBUS_PLUGINS_ICSNEOCAN, "Cannot apply parameter %d with value %ls.",
                      key, qUtf16Printable(param.toString()));
        }
#endif
    }
    setState(QCanBusDevice::ConnectedState);
    return true;
}

void IcsNeoCanBackend::close()
{
    Q_D(IcsNeoCanBackend);
    d->close();
    setState(QCanBusDevice::UnconnectedState);
}

void IcsNeoCanBackend::setConfigurationParameter(int key, const QVariant &value)
{
    Q_D(IcsNeoCanBackend);

    if (d->setConfigurationParameter(key, value))
        QCanBusDevice::setConfigurationParameter(key, value);
}

bool IcsNeoCanBackend::writeFrame(const QCanBusFrame &newData)
{
    Q_D(IcsNeoCanBackend);

    if (Q_UNLIKELY(state() != QCanBusDevice::ConnectedState))
        return false;

    if (Q_UNLIKELY(!newData.isValid())) {
        setError(tr("Cannot write invalid QCanBusFrame"), QCanBusDevice::WriteError);
        return false;
    }

    const QCanBusFrame::FrameType type = newData.frameType();
    if (Q_UNLIKELY(type != QCanBusFrame::DataFrame && type != QCanBusFrame::RemoteRequestFrame)) {
        setError(tr("Unable to write a frame with unacceptable type"),
                 QCanBusDevice::WriteError);
        return false;
    }

    // CAN FD frame format is not supported by the hardware yet
    if (Q_UNLIKELY(newData.hasFlexibleDataRateFormat())) {
        setError(tr("CAN FD frame format not supported"), QCanBusDevice::WriteError);
        return false;
    }

    enqueueOutgoingFrame(newData);
    d->enableWriteNotification(true);
    return true;
}

void IcsNeoCanBackend::resetController()
{
    Q_D(IcsNeoCanBackend);
    d->resetController();
}

QCanBusDevice::CanBusStatus IcsNeoCanBackend::busStatus()
{
    Q_D(IcsNeoCanBackend);

    return d->busStatus();
}

QT_END_NAMESPACE
