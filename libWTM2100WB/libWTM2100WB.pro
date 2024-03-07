QT -= gui
QT       += serialport

TEMPLATE = lib
CONFIG += staticlib

CONFIG += c++17
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
    src/LibWTM2100WB.cpp \
    src/Settings.cpp \
    src/FlashThread.cpp

HEADERS += \
    include/LibWTM2100WB.h \
    include/Settings.h \
    include/libMPSSE_spi.h \
    include/FlashThread.h

# Default rules for deployment.
unix {
    target.path = $$[QT_INSTALL_PLUGINS]/generic
}
!isEmpty(target.path): INSTALLS += target
