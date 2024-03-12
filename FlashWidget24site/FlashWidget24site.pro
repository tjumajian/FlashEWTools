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

RESOURCES += \
            resources/res.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$_PRO_FILE_PWD_/../libWTM2100WB/include
DEPENDPATH += $$_PRO_FILE_PWD_/../libWTM2100WB/include
Flash {
TARGET = FlashWidget24Sites
LIBS += -L$$OUT_PWD/../lib/ -lWTM2100WBFlash
PRE_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBFlash.a
POST_TARGETDEPS += $$OUT_PWD/../lib/libWTM2100WBFlash.a
}

DISTFILES += \
    resources/icon.ico