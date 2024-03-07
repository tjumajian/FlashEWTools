QT       += core gui
QT       += serialport
QT       += widgets


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
DESTDIR = $$OUT_PWD/../bin/ui

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    triangle.cpp \
    flashwindow.cpp

HEADERS += \
    flashwindow.h \
    triangle.h

FORMS += \
    flashwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$_PRO_FILE_PWD_/../libWTM2100WB/include
DEPENDPATH += $$_PRO_FILE_PWD_/../libWTM2100WB/include
CP2 {
TARGET = SerialPortTestCP2
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBCP2
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP2.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP2.a
}
CP3 {
TARGET = SerialPortTestCP3
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBCP3
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP3.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP3.a
}
Flash {
TARGET = FlashWidget24Sites
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBFlash
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBFlash.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBFlash.a
}
CP_debug {
TARGET = SerialPortTestCP_debug
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBCP_debug
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP_debug.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP_debug.a
}
CommonUI {
TARGET = SerialPortTest
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WB
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WB.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WB.a
}