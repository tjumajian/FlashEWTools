QT -= gui
QT       += serialport

CONFIG += c++17 console
CONFIG -= app_bundle
DESTDIR = $$OUT_PWD/../bin/WTM2100WB

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$_PRO_FILE_PWD_/../libWTM2100WB/include
DEPENDPATH += $$_PRO_FILE_PWD_/../libWTM2100WB/include
CP2 {
TARGET = WTM2100WB_CP2_24sites_test
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBCP2
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP2.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP2.a
}
CP3 {
TARGET = WTM2100WB_CP3_24sites_test
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBCP3
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP3.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBCP3.a
}
Flash {
TARGET = WTM2100WB_Flash_24sites_test
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBFlash
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBFlash.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBFlash.a
}
CommonUI {
TARGET = WTM2100WB_24sites_test
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WB
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WB.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WB.a
}
