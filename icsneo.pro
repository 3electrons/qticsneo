BUILD_FLAGS = 3rd_ftdi

QT = core serialbus

PLUGIN_TYPE = canbus
PLUGIN_CLASS_NAME = IcsNeoCanBusPlugin
TARGET = qticsneocanbus
TEMPLATE = lib
CONFIG += plugin
DESTDIR = plugins/canbus
DISTFILES = icsneo.json

INCLUDEPATH = libicsneo/include \
              generated

MOC_DIR = objects
OBJECTS_DIR = objects

ICSNEO_SOURCES +=   libicsneo/api/icsneocpp/event.cpp \
                    libicsneo/api/icsneocpp/eventmanager.cpp \
                    libicsneo/api/icsneocpp/icsneocpp.cpp \
                    libicsneo/api/icsneocpp/version.cpp \
                    libicsneo/communication/packet/canpacket.cpp \
                    libicsneo/communication/packet/ethernetpacket.cpp \
                    libicsneo/communication/packet/flexraypacket.cpp \
                    libicsneo/communication/packet/iso9141packet.cpp \
                    libicsneo/communication/decoder.cpp \
                    libicsneo/communication/encoder.cpp \
                    libicsneo/communication/ethernetpacketizer.cpp \
                    libicsneo/communication/packetizer.cpp \
                    libicsneo/communication/multichannelcommunication.cpp \
                    libicsneo/communication/communication.cpp \
                    libicsneo/communication/driver.cpp \
                    libicsneo/communication/message/flexray/control/flexraycontrolmessage.cpp \
                    libicsneo/communication/message/neomessage.cpp \
                    libicsneo/device/extensions/flexray/controller.cpp \
                    libicsneo/device/extensions/flexray/extension.cpp \
                    libicsneo/device/idevicesettings.cpp \
                    libicsneo/device/devicefinder.cpp \
                    libicsneo/device/device.cpp

unix{

     QMAKE_CXXFLAGS += -Wno-sign-compare -Wno-unused-parameter -Wno-switch -Wno-missing-field-initializers -Wno-implicit-fallthrough  #to get rid of annnoying icsneo warrnings
     QMAKE_CFLAGS = $$QMAKE_CXXFLAGS

     CONFIG    += link_pkgconfig object_parallel_to_source

     PKGCONFIG += libpcap

     #LIBS+=             libicsneo/build/third-party/libftdi/src/libftdi1.a

     ICSNEO_SOURCES +=   libicsneo/platform/posix/ftdi.cpp \
                         libicsneo/platform/posix/pcap.cpp \
                         libicsneo/platform/posix/stm32.cpp \
                         libicsneo/platform/posix/linux/stm32linux.cpp


    contains(BUILD_FLAGS, 3rd_ftdi){

        INCLUDEPATH +=     libicsneo/third-party/libftdi/src
                           libicsneo/third-party/libftdi/ftdipp

        ICSNEO_SOURCES += libicsneo/third-party/libftdi/ftdipp/ftdi.cpp \
                          libicsneo/third-party/libftdi/src/ftdi_stream.c \
                          libicsneo/third-party/libftdi/src/ftdi.c
    }

    contains(BUILD_FLAGS, system_ftdi){
       PKGCONFIG += libftdi1
    }
    else
       PKGCONFIG += libusb-1.0

}

win32-gcc{
    QMAKE_CXXFLAGS += -std=c++17 -Wno-sign-compare -Wno-unused-parameter -Wno-switch -Wno-missing-field-initializers -Wimplicit-fallthrough=0  #to get rid of annnoying icsneo warrnings
    QMAKE_CFLAGS = $$QMAKE_CXXFLAGS
    CONFIG+= object_parallel_to_source
}

win32-msvc*{
   QMAKE_CXXFLAGS+=  /Zc:strictStrings-   #thid-party/wincap/pcap.cpp generated C2664 error
}

win32{

    QMAKE_CXXFLAGS +=   -DWPCAP -DHAVE_REMOTE -DWIN32_LEAN_AND_MEAN   # to force including Win32-Extensions from pcap.h in third-party/winpcap/inlclude
    INCLUDEPATH    +=   libicsneo/third-party/optional-lite/include \
                        libicsneo/third-party/winpcap/include

    ICSNEO_SOURCES +=   libicsneo/platform/windows/internal/pcapdll.cpp \
                        libicsneo/platform/windows/pcap.cpp \
                        libicsneo/platform/windows/vcp.cpp \
                        libicsneo/platform/windows/registry.cpp

    LIBS+= -liphlpapi  -lAdvapi32
}


HEADERS += icsneo_plugin.h \
           icsneocanbackend.h \
           icsneocanbackend_p.h

SOURCES += icsneocanbackend.cpp \
            $$ICSNEO_SOURCES



