QT -= gui
QT       += serialport

TEMPLATE = lib
CONFIG += staticlib

CONFIG += c++17
TARGET = WTM2100WB
CP2 {
TARGET = WTM2100WBCP2
DEFINES += SETTINGS_PATH=\"\\\"settings_cp2.json\\\"\"
message("CP2")
}
CP3 {
TARGET = WTM2100WBCP3
DEFINES += SETTINGS_PATH=\"\\\"settings_cp3.json\\\"\"
message("CP3")
}
Flash {
TARGET = WTM2100WBFlash
DEFINES += SETTINGS_PATH=\"\\\"settings_Flash.json\\\"\"
message("Flash")
}
DESTDIR = $$OUT_PWD/../lib

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/BinJudge.cpp \
    src/LibWTM2100WB.cpp \
    src/NPUProgramAccuracyLowData.cpp \
    src/Settings.cpp \
    src/UartThread.cpp \
    src/FlashThread.cpp

HEADERS += \
    include/BinJudge.h \
    include/LibWTM2100WB.h \
    include/Settings.h \
    include/UartThread.h \
    include/NPUProgramAccuracyLowData.h \
    include/ftd2xx.h \
    include/libMPSSE_spi.h \
    include/FlashThread.h

# Default rules for deployment.
unix {
    target.path = $$[QT_INSTALL_PLUGINS]/generic
}
!isEmpty(target.path): INSTALLS += target
