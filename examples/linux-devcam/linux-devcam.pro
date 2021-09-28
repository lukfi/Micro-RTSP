TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += USE_LFF

INCLUDEPATH += ../../src
INCLUDEPATH += ../../../Common/System ../../../Common/Multimedia

SOURCES += \
        main.cpp \
    ../../src/SimStreamer.cpp \
    ../../src/CStreamer.cpp \
    ../../src/JPEGSamples.cpp \
    ../../src/CRtspSession.cpp \
    camerastreamer.cpp

CONFIG(debug, debug|release) {
    LIBS = -L../../../CommonLibs/debug
} else {
    LIBS = -L../../../CommonLibs/release
}

LIBS += -lMultimedia -lSystem -latomic -lpthread
unix {
    LIBS += -ljpeg -lv4l2
}

HEADERS += \
    platglue-lf.h \
    ../../src/SimStreamer.h \
    ../../src/CStreamer.h \
    ../../src/JPEGSamples.h \
    ../../src/CRtspSession.h \
    camerastreamer.h
