/****************************************************************************
** Copyright (C) 2021  Tomasz Ziobrowski <t.ziobrowski@3electrons.com>
****************************************************************************/


#include "icsneocanbackend.h"
#include "icsneocanbackend_p.h"
#include "icsneo/icsneocpp.h"
#include "include/qticsneo_keys.h"

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
QMap<QString, std::shared_ptr<icsneo::Device>> IcsNeoCanBackendPrivate::m_devices;

IcsNeoCanBackendPrivate::IcsNeoCanBackendPrivate(IcsNeoCanBackend *q) :
    q_ptr(q),
    incomingEventHandler(new IncomingEventHandler(this, q))
{
}


bool IcsNeoCanBackendPrivate::setupDevice()
{
    Q_Q(IcsNeoCanBackend);
    if(!m_device)
        return false;

    if (!m_device->settings->refresh())
        resetController();

    QVariant value;
    bool res = m_device!=nullptr;

    // Loopback
    value = q->configurationParameter(QCanBusDevice::LoopbackKey);
    if (value.isValid() && res)
    {
        CAN_SETTINGS * set = m_device->settings->getMutableCANSettingsFor(m_network);
        if (set)
           value.toBool() ? set->Mode = LOOPBACK : set->Mode = NORMAL;
        else
            res = false;
    }

    // BitRate
    value = q->configurationParameter(QCanBusDevice::BitRateKey);
    if (value.isValid() && res)
        res &= m_device->settings->setBaudrateFor(m_network, value.toInt()) ;

    // CanFD Settings
    value = q->configurationParameter(QCanBusDevice::CanFdKey);
    if (value.toBool() && res)
    {
         /*  NO_CANFD = 0 , CANFD_ENABLED=1, CANFD_BRS_ENABLED=2, CANFD_ENABLED_ISO=3, CANFD_BRS_ENABLED_ISO=4 */
         CANFD_SETTINGS * canFD = m_device->settings->getMutableCANFDSettingsFor(m_network);

         if (!canFD)
             res = false;
         // Iso
         value =  q->configurationParameter(ParameterIsoKey);
         if (value.isValid() && res)
            value.toBool() ? canFD->FDMode = CANFD_BRS_ENABLED_ISO : canFD->FDMode = CANFD_BRS_ENABLED;
         //res&=m_device->settings->apply();

         // FD-Bitrate
         value = q->configurationParameter(QCanBusDevice::DataBitRateKey);
         if (value.isValid() && res)
              res &= m_device->settings->setFDBaudrateFor(m_network, value.toInt())  ;
         //res&=m_device->settings->apply();

         // Termination
         value = q->configurationParameter(ParameterTerminationKey);
         if (value.isValid() && res && m_device->settings->canTerminationBeEnabledFor(m_network))
                m_device->settings->setTerminationFor(m_network,value.toBool());
         //res&=m_device->settings->apply();
     }
      else
      if (res) m_device->settings->getMutableCANFDSettingsFor(m_network)->FDMode = NO_CANFD;

      value = q->configurationParameter(ParameterFlashKey);
      if (res ) res &=m_device->settings->apply() ; // !value.toBool());

    return res;
}

bool IcsNeoCanBackendPrivate::open()
{
    Q_Q(IcsNeoCanBackend);

    if (!m_device.get()) // device does not exist anymore
        return false;

    bool res =  m_device->open();
    if (res) res &= setupDevice();
    if (res) res &= m_device->goOnline();

    if (nullptr == m_device)
        return false;

    if (!res)
    {
        m_device->close();
        q->setError(QString::fromStdString(icsneo::GetLastError().describe()),
                    QCanBusDevice::ConnectionError);
    }
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

bool IcsNeoCanBackendPrivate::setConfigurationParameter(int key, const QVariant &value)
{
    Q_Q(IcsNeoCanBackend);
    switch (key)
    {
        case QCanBusDevice::BitRateKey:       return true;
        case QCanBusDevice::DataBitRateKey:   return true;
        case QCanBusDevice::CanFdKey:         return true;
        case QCanBusDevice::LoopbackKey:      return true;
        case ParameterIsoKey :                return true;
        case ParameterTerminationKey:         return true;
        case ParameterFlashKey:               return true;
        case QCanBusDevice::ReceiveOwnKey:
        {
            if (Q_UNLIKELY(q->state() != QCanBusDevice::UnconnectedState))
            {
                q->setError(IcsNeoCanBackend::tr("Cannot configure TxEcho for open device"),
                            QCanBusDevice::ConfigurationError);
                return false;
            }
            return true;
        }
        default:
        {
            q->setError(IcsNeoCanBackend::tr("Unsupported configuration key: %1").arg(key),
                        QCanBusDevice::ConfigurationError);
            return false;
        }
    }
}

bool IcsNeoCanBackendPrivate::setupChannel(const QString &interfaceName)
{
    Q_Q(IcsNeoCanBackend);

    const QRegularExpression re(QStringLiteral("can(\\d)\\.(\\d)"));
    const QRegularExpressionMatch match = re.match(interfaceName);

    if (Q_LIKELY(match.hasMatch()) && m_device)
    {
        device = quint8(match.captured(1).toUShort());
        channel = quint8(match.captured(2).toUShort());     
        m_network = m_device->getNetworkByNumber(icsneo::Network::Type::CAN, channel+1);
    }
    else
    {
        q->setError(IcsNeoCanBackend::tr("Invalid interface '%1'.")
                    .arg(interfaceName), QCanBusDevice::ConnectionError);
        return false;
    }
    return true;
}

void IcsNeoCanBackendPrivate::setupDefaultConfigurations()
{
    Q_Q(IcsNeoCanBackend);

    if (!m_device)
        return;

    bool open = m_device->open();
    Q_UNUSED(open);
    int bitrate = m_device->settings->getBaudrateFor(m_network);
    int fdbitrate = m_device->settings->getFDBaudrateFor(m_network);

    const CANFD_SETTINGS * canFD = m_device->settings->getCANFDSettingsFor(m_network);
    const CAN_SETTINGS * can = m_device->settings->getCANSettingsFor(m_network);

    if (can && canFD)
    {
     bool hasloopback = can->Mode & LOOPBACK;
     q->setConfigurationParameter(QCanBusDevice::LoopbackKey, hasloopback);

     bool hasCanFD = canFD->FDMode!= NO_CANFD;
     q->setConfigurationParameter(QCanBusDevice::CanFdKey, hasCanFD);

     bool hasIso = CANFD_BRS_ENABLED_ISO & canFD->FDMode;
     q->setConfigurationParameter(ParameterIsoKey, hasIso);

     q->setConfigurationParameter(ParameterFlashKey,false);

     if (m_device->settings->canTerminationBeEnabledFor(m_network))
     {
        bool hasTermination = m_device->settings->isTerminationEnabledFor(m_network).value();
        q->setConfigurationParameter(ParameterTerminationKey, hasTermination);
     }

     q->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrate);       // standard BitRate
     q->setConfigurationParameter(QCanBusDevice::DataBitRateKey, fdbitrate);

    }
    m_device->close();
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

    auto msg                 = std::make_shared<icsneo::CANMessage>();
    msg->network             = m_network;
    msg->arbid               = frame.frameId();
    msg->isRemote            = frame.frameType() == QCanBusFrame::RemoteRequestFrame;
    msg->isCANFD             = m_hasFD;
    msg->isExtended          = frame.hasExtendedFrameFormat();
    msg->baudrateSwitch      = frame.hasBitrateSwitch();
    msg->errorStateIndicator = frame.hasErrorStateIndicator();
    msg->data.insert(msg->data.end(), payload.begin(), payload.end()) ;

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
    //@TODO this is generic implementation based on Qt CanBus module will not work till polling is not enabled
    // Attempt to get messages, limiting the number of messages at once to 50,000
    // A third parameter of type std::chrono::milliseconds is also accepted if a timeout is desired

    Q_Q(IcsNeoCanBackend);
    QVector<QCanBusFrame> newFrames;
    std::vector<std::shared_ptr<icsneo::Message>> msgs;
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


void IcsNeoCanBackendPrivate::resetController()
{

    auto uniqDevString =[=](const std::shared_ptr<icsneo::Device> & device ) {
        QString str = QString::fromStdString(device->describe()) + QString(" - %1").arg(typeid(m_device).name());
        str+= QString::fromStdString(device->getSerial());
        return str;
    };

    qCWarning(QT_CANBUS_PLUGINS_ICSNEOCAN, "Reseting controller");

    Q_Q(IcsNeoCanBackend);
    QString description = uniqDevString(m_device);

    this->close();              // Close current connection

    m_device->settings->applyDefaults(); // setupd default settings
    m_device->settings->apply();         // write into device EEPROM memory

    delete m_device.get();      // Delete current device
    m_device.reset();           // Reset variable

    // Restore device handler
    std::vector<std::shared_ptr<icsneo::Device>>  devices = icsneo::FindAllDevices();
    for (auto dev : devices)
    {
        QString dev_description =   uniqDevString(dev);


       if (dev_description == description )
       {
           m_device = dev ;
           dev.reset();
           break;
       }
    }

    if (nullptr == m_device )
    {
        q->setError(IcsNeoCanBackend::tr("Cannot reset controller. Try unplug and plug deivce again"),
                    QCanBusDevice::UnknownError);
        return;
    }

    // Resored opened state;
    if (q->state() == QCanBusDevice::ConnectedState)
    {
       q->setState(QCanBusDevice::UnconnectedState);
       q->open();
    }
}

QCanBusFrame IcsNeoCanBackendPrivate::interpretFrame( icsneo::CANMessage * msg )
{
    if (!(icsneo::Network::Type::CAN == msg->network.getType() &&
          msg->network.getNetID() == m_network.getNetID() ) )
        return QCanBusFrame(QCanBusFrame::InvalidFrame);

    QByteArray data;
    for (auto b : msg->data)
        data.append(b);

    QCanBusFrame frame(msg->arbid, data);

    //qDebug() << "Message timestamp:" << msg->timestamp << " Sec:" << msg->timestamp / 1000000 << "." << msg->timestamp % 1000000;
    frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(msg->timestamp));
    frame.setExtendedFrameFormat(msg->isExtended);
    frame.setFlexibleDataRateFormat(msg->isCANFD);
    frame.setErrorStateIndicator(msg->errorStateIndicator);

    //frame.setLocalEcho(msg->transmited); What does it do?

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

    icsneo::APIEvent event = icsneo::GetLastError();
    if (event.getDevice() == m_device.get() && event.getSeverity() == icsneo::APIEvent::Severity::Error)
    {        
          QString error = QString::fromStdString(event.describe());
          q->setError(error, QCanBusDevice::ConfigurationError);
          return QCanBusDevice::CanBusStatus::Error;
    }

    QStringList  warnings;
    for( auto & event : GetEvents(icsneo::EventFilter()) )
          warnings <<  QString::fromStdString(event.describe());

   // if (q->configurationParameter(ParameterOmitKey).toBool())
   //    warnings << "Configuration ommited - using device defaults";

    if (!warnings.isEmpty())
    {
        for(auto msg: warnings)
            qCWarning(QT_CANBUS_PLUGINS_ICSNEOCAN, "Warning: %ls", qUtf16Printable(msg));

        q->setError(warnings.join("\n"), QCanBusDevice::ConfigurationError);
        return QCanBusDevice::CanBusStatus::Warning;
    }

    if (m_device->isOnline())
        return QCanBusDevice::CanBusStatus::Good;
    else
        return QCanBusDevice::CanBusStatus::BusOff;

    return QCanBusDevice::CanBusStatus::Unknown;
}

void IcsNeoCanBackendPrivate::interfaces( QList<QCanBusDeviceInfo> & list)
{
    m_devices.clear();
    std::vector<std::shared_ptr<icsneo::Device>>  devices = icsneo::FindAllDevices();
    for (unsigned int i = 0 ; i < devices.size() ; i++)
    {
        std::shared_ptr<icsneo::Device> & device = devices[i];
        int channels = device->getNetworkCountByType(icsneo::Network::Type::CAN);
        for (int channel = 0 ; channel < channels ; channel++)
        {
            QString description =   QString::fromStdString(device->describe()) + QString(" - %1").arg(typeid(*device).name());
            QString serial      = QString::fromStdString(device->getSerial());
            QCanBusDeviceInfo info = IcsNeoCanBackend::createDeviceInfo(serial, description, i, channel);
            m_devices[info.name()] = device;
            list.append(std::move(info));
        }
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
    d_ptr->m_device = IcsNeoCanBackendPrivate::m_devices[name];
    d->setupChannel(name);
    d->setupDefaultConfigurations();
    std::function<void()> f = std::bind(&IcsNeoCanBackend::resetController, this);
    setResetControllerFunction(f);
    std::function<CanBusStatus()> g = std::bind(&IcsNeoCanBackend::busStatus, this);
    setCanBusStatusGetter(g);
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
    //@TODO - this is stupid ... and not usable as error frame is not set anywhere
    switch (errorFrame.error())
    {
    case QCanBusFrame::TransmissionTimeoutError   : return "Transmission timeout";
    case QCanBusFrame::LostArbitrationError       : return "Lost arbitration";
    case QCanBusFrame::ControllerError            : return "Controller error";
    case QCanBusFrame::ProtocolViolationError     : return "Protocol violation";
    case QCanBusFrame::TransceiverError           : return "Trensceiver error";
    case QCanBusFrame::MissingAcknowledgmentError : return "Missing Acknowledgment";
    case QCanBusFrame::BusOffError                : return "Bus off";
    case QCanBusFrame::BusError                   : return "Bus error";
    case QCanBusFrame::ControllerRestartError     : return "Controller restart fail";
    case QCanBusFrame::UnknownError               : return "Unknown error";
    case QCanBusFrame::AnyError                   : return "AnyError";
    }

    return QString();
}

bool IcsNeoCanBackend::open()
{
    Q_D(IcsNeoCanBackend);
    if (!d->open())
        return false;
    setState(QCanBusDevice::ConnectingState);
    // Apply all stored configurations except bitrate and receive own,
    // because these cannot be applied after opening the device
    const QVector<int> keys = configurationKeys();
    for (int key : keys) {
        if (key == BitRateKey || key == ReceiveOwnKey)
            continue;
        const QVariant param = configurationParameter(key);
        const bool success = d->setConfigurationParameter(key, param);

        if (Q_UNLIKELY(!success)) {
            qCWarning(QT_CANBUS_PLUGINS_ICSNEOCAN, "Cannot apply parameter %d with value %ls.",
                      key, qUtf16Printable(param.toString()));
        }

    }
    setState(QCanBusDevice::ConnectedState);
    return true;
}

void IcsNeoCanBackend::close()
{
    Q_D(IcsNeoCanBackend);
    setState(QCanBusDevice::ClosingState);
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
