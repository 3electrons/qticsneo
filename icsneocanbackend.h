/****************************************************************************
** Copyright (C) 2021  Tomasz Ziobrowski <t.ziobrowski@3electrons.com>
****************************************************************************/

#ifndef ICSNEOCANBACKEND_H
#define ICSNEOCANBACKEND_H

#include <QtSerialBus/qcanbusframe.h>
#include <QtSerialBus/qcanbusdevice.h>
#include <QtSerialBus/qcanbusdeviceinfo.h>

#include <QtCore/qvariant.h>
#include <QtCore/qlist.h>


QT_BEGIN_NAMESPACE

class IcsNeoCanBackendPrivate;

class IcsNeoCanBackend : public QCanBusDevice
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(IcsNeoCanBackend)
    Q_DISABLE_COPY(IcsNeoCanBackend)
public:
    explicit IcsNeoCanBackend(const QString &name, QObject *parent = nullptr);
    ~IcsNeoCanBackend();
    // This function needs to be public as it is accessed by a callback
    static QCanBusDeviceInfo createDeviceInfo(const QString &serialNumber,
                                              const QString &description,
                                              uint deviceNumber,
                                              int channelNumber);

    static QList<QCanBusDeviceInfo> interfaces();

    QString interpretErrorFrame(const QCanBusFrame &errorFrame) override;

    bool open() override;
    void close() override;
    void setConfigurationParameter(int key, const QVariant &value) override;
    bool writeFrame(const QCanBusFrame &newData) override;

private:
    void resetController();
    QCanBusDevice::CanBusStatus busStatus();

    IcsNeoCanBackendPrivate * const d_ptr;
};

QT_END_NAMESPACE

#endif // ICSNEOCANBACKEND_H
