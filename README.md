# qticsneo
### The Qt CAN-BUS plugin for Intrepid Control Systems 
An open source solution to integrate Intrepid Control Systems vehicle networking hardware with your Qt application based on [slightly modified libicsneo libary](https://github.com/3electrons/libicsneo)

## Introduction 
This is very initial stage of the plugin - not all funtctions or checks are supported it was implemented on other plugin example. 
Whole libicsneo is "staticaly" complied into plugin. Using dynamic version of libicsneo was impractical and C++ interface is not recommended for such use by its [authors](https://github.com/3electrons/libicsneo#dll--so--dylib-releases-dynamic-linking). 
QMake is used as build system. 

## Supported and tested configurations 
### Linux:
- Gcc 10.2.0 
### Windows:
- MinGw32 8.1
- Microsoft Visual Studio 2017 64Bit

## Builiding from source and usage
``` bash
git clone https://github.com/3electrons/qticsneo.git --recursive
cd qticsneo 
qmake  
make 
```
Build plugin will be genterated into plugins/canbus directory. 
The simples way to test it, is use it against [serialbus/can](https://doc.qt.io/qt-5/qtserialbus-can-example.html) example from Qt/Examples directory in Qt SDK. 

## Dependencies 
### Windows
The dependencies are as follows:
- Qt SDK
- Some C++ complier supporting above Qt version (MinGW, MSVC) 

### Linux
The dependencies are as follows:
 - GCC
 - Qt SDK 
 - `libusb-1.0-0-dev`
 - `libpcap0.8-dev`
 - `build-essential` is recommended

## Changelog 
### Release 2021.06.14 
- Added channels listed as CANx.y where X is Interpids device and Y is Network/Channel number on device such as HSCAN1, HSCANFD2 
- Added CANFD support 
- Shadow buildng works 
- Verification of bitrate is done by libicsneo - returning nice error messages 
- Added License file

### Release 2021.06.12 
- Proof of concept - basic functionality. 

### Still TODO
- Verbose logging (No logging at all on Windows)
- Consoder implementation of IncomingEvenHandler (mockup already exists) with setConfigurationParameter(QCanBusDevice::UserKey + PollingQueue ) to possibly process heavy loads. 
- Not shure if QCanBusDevice::LoopbackKey is properly implemented - using CAN_SETTINGS->Mode = LOOPBACK. 
- Verify if device status could be better implemnted - Possibly by events? 
- Implement resetDevice (now it is mockup only) 
- Consider adding AutoBaudKey as own Key to support CAN_SETTINGS::auto_baud. More info form libicsneo needed. 
- Implement configuration parameters QCanBusDevice::RecieveOwnKey, QCanBusDevice::RawFilterKey, QCanBusDevice::ErrorFilterKey, 
