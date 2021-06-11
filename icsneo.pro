#load(qt_plugin)

QT = core serialbus

TARGET = qticsneocanbus
TEMPLATE = lib
CONFIG += plugin

INCLUDEPATH = libicsneo/include

MOC_DIR = objects
OBJECTS_DIR = objects

LIBS+= libicsneo/build/libicsneoc-static.a \
       libicsneo/build/third-party/libftdi/src/libftdi1.a \
       -lpcap -lusb-1.0



HEADERS += icsneo_plugin.h \
           icsneocanbackend.h \
          icsneocanbackend_p.h \
    libicsneo/include/icsneo/api/event.h \
    libicsneo/include/icsneo/api/eventcallback.h \
    libicsneo/include/icsneo/api/eventmanager.h \
    libicsneo/include/icsneo/api/version.h

SOURCES += icsneocanbackend.cpp \
    libicsneo/api/icsneocpp/event.cpp \
    libicsneo/api/icsneocpp/eventmanager.cpp \
    libicsneo/api/icsneocpp/icsneocpp.cpp \
    libicsneo/api/icsneocpp/version.cpp

DISTFILES = icsneo.json
PLUGIN_TYPE = canbus
PLUGIN_CLASS_NAME = IcsNeoCanBusPlugin

