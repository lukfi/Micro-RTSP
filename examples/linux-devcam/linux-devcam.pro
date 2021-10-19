TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += USE_LFF

INCLUDEPATH += ../../src
INCLUDEPATH += ../../../Common/System ../../../Common/Multimedia

HEADERS += \
    platglue-lf.h \
    ../../src/SimStreamer.h \
    ../../src/CStreamer.h \
    ../../src/JPEGSamples.h \
    ../../src/CRtspSession.h \
    ../../src/JPEGAnalyse.h \
    camerastreamer.h \
    rtspstreamserver.h

SOURCES += \
        main.cpp \
    ../../src/SimStreamer.cpp \
    ../../src/CStreamer.cpp \
    ../../src/JPEGSamples.cpp \
    ../../src/CRtspSession.cpp \
    ../../src/JPEGAnalyse.cpp \
    camerastreamer.cpp \
    rtspstreamserver.cpp

CONFIG(debug, debug|release) {
    LIBS = -L../../../CommonLibs/debug
} else {
    LIBS = -L../../../CommonLibs/release
}

LIBS += -lMultimedia -lSystem -latomic -lpthread
unix {
    LIBS += -ljpeg -lv4l2
}

win32 {
    INCLUDEPATH += ../../../3rdPart/jpeg/9c/win64/include
    SOURCES += ../../../Common/Multimedia/graphics/jpeg.cpp
    LIBS += -L../../../3rdPart/jpeg/9c/win64/bin
    LIBS += -ljpeg-9
}
