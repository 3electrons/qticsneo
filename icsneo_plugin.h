#ifndef ICSNEOPLUGIN_G
#define ICSNEOPLUGIN_G

#include <QCanBusFactoryV2>
#include "icsneocanbackend.h"

class IcsNeoCanBusPlugin : public QObject, public QCanBusFactoryV2
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QCanBusFactory" FILE "icsneo.json")
    Q_INTERFACES(QCanBusFactoryV2)

public:
    QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage) const override
    {
        Q_UNUSED(errorMessage);
        return  IcsNeoCanBackend::interfaces();
    }

    QCanBusDevice *createDevice(const QString &interfaceName, QString *errorMessage) const override
    {
        Q_UNUSED(errorMessage);
        auto device = new IcsNeoCanBackend(interfaceName);
        return device;
    }


};

#endif // ICSNEOPLUGIN_G
