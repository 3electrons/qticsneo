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


### Still TODO
- Support for multiple channels (only one channel supported at the noment) 
- Verbose logging 
- Shadow building 
- Verify channel distinguish when starting or multiplexing particular device and add channel support 
- Verification of bitrate according to icsneo enum types
- Remove incoming event handler as it is not needed 
- Implement all configuration / parameters Keys with its proper support during opening device (Loopback, DataBitRate Key ) 
- Verify if device status could be better implemnted
- Implement resetDevice (now it is mockup only) 
