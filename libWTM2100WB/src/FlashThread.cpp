#include "../include/FlashThread.h"
#include "../include/Settings.h"
#include "../include/BinJudge.h"
#include "../include/LibWTM2100WB.h"
#include "../include/libMPSSE_spi.h"

#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <iostream>
#include <cstdlib>
#include <iomanip>
using namespace std;
#include <cstring>
#include <thread>
#include <fstream>

#define APP_CHECK_STATUS(exp)                                       \
    {                                                               \
        if (exp != FT_OK)                                           \
        {                                                           \
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): status(0x" << std::hex << exp << ") != FT_OK" << std::endl; \
            std::exit(1);                                           \
        }                                                           \
        else                                                        \
        {                                                           \
            ;                                                       \
        }                                                           \
    };

#define CHECK_NULL(exp)                                             \
    {                                                               \
        if (exp == NULL)                                            \
        {                                                           \
            std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): NULL expression encountered" << std::endl; \
            std::exit(1);                                           \
        }                                                           \
        else                                                        \
        {                                                           \
            ;                                                       \
        }                                                           \
    };

#define CHANNEL_TO_OPEN  0  /*0 for first available channel, 1 for next... */

/* Application specific macro definations */
#define SPI_DEVICE_BUFFER_SIZE		256
#define SPI_WRITE_COMPLETION_RETRY		10
#define START_ADDRESS_EEPROM 	0x00 /*read/write start address inside the EEPROM*/
#define END_ADDRESS_EEPROM		0x10
#define RETRY_COUNT_EEPROM		10	/* number of retries if read/write fails */
#define SPI_SLAVE_0				0
#define SPI_SLAVE_1				1
#define SPI_SLAVE_2				2
#define DATA_OFFSET				4
#define USE_WRITEREAD			0

static FT_HANDLE handlea;
static uint8_t buffer[SPI_DEVICE_BUFFER_SIZE] = {0};

static QLibrary *s_libraryMPSSE = nullptr;
static QLibrary *s_libraryTunnel = nullptr;
typedef void (*TunnelFuncFlash)(uint32_t* ReadyArray);
void (*set_ready_arrFlash)(uint32_t* ReadyArray);
void (*get_ready_arrFlash)(uint32_t* ReadyArray);

typedef void (*Init_libMPSSE_ptr)(void);
typedef void (*Cleanup_libMPSSE_ptr)(void);
typedef FT_STATUS (*SPI_GetNumChannels_ptr)(DWORD *);
typedef FT_STATUS (*SPI_GetChannelInfo_ptr)(DWORD , FT_DEVICE_LIST_INFO_NODE *);
typedef FT_STATUS (*SPI_OpenChannel_ptr)(DWORD , FT_HANDLE *);
typedef FT_STATUS (*SPI_CloseChannel_ptr)(FT_HANDLE);
typedef FT_STATUS (*SPI_InitChannel_ptr)(FT_HANDLE, ChannelConfig *);
typedef FT_STATUS (*FT_SPI_InitChannel_ptr)(void*, unsigned char , unsigned char );
typedef FT_STATUS (*FT_WriteGPIO_ptr)(void*, unsigned char , unsigned char );
typedef FT_STATUS (*FT_ReadGPIO_ptr)(void*, unsigned char *);
typedef FT_STATUS (*Mid_SetGPIOLow_ptr)(FT_HANDLE , uint8_t , uint8_t );
typedef FT_STATUS (*SPI_Read_ptr)(FT_HANDLE, uint8_t *, uint32_t, uint32_t *, uint32_t);
typedef FT_STATUS (*SPI_Write_ptr)(FT_HANDLE, uint8_t *, uint32_t, uint32_t *, uint32_t);
typedef FT_STATUS (*Mid_CyclePort_ptr)(FT_HANDLE );


void (*Init_libMPSSE)(void);
void (*Cleanup_libMPSSE)(void);
FT_STATUS (*SPI_GetNumChannels)(DWORD *);
FT_STATUS (*SPI_GetChannelInfo)(DWORD , FT_DEVICE_LIST_INFO_NODE *);
FT_STATUS (*SPI_OpenChannel)(DWORD , FT_HANDLE *);
FT_STATUS (*SPI_CloseChannel)(FT_HANDLE);
FT_STATUS (*SPI_InitChannel)(FT_HANDLE, ChannelConfig *);
FT_STATUS (*FT_SPI_InitChannel)(void*, unsigned char , unsigned char );
FT_STATUS (*FT_WriteGPIO)(void*, unsigned char , unsigned char );
FT_STATUS (*FT_ReadGPIO)(void*, unsigned char *);
FT_STATUS (*Mid_SetGPIOLow)(FT_HANDLE , uint8_t , uint8_t );
FT_STATUS (*SPI_Read)(FT_HANDLE, uint8_t *, uint32_t, uint32_t *, uint32_t);
FT_STATUS (*SPI_Write)(FT_HANDLE, uint8_t *, uint32_t, uint32_t *, uint32_t);
FT_STATUS (*Mid_CyclePort)(FT_HANDLE);

FlashThread::FlashThread( QObject *parent)
    : QThread{parent}
{
    if (s_libraryMPSSE == nullptr)
    {
        QString mpsseDllPath = Settings::staticInstance()->value("MPSSEDllPath").toString();
        bool ok = false;
        s_libraryMPSSE = new QLibrary();
        if (!mpsseDllPath.isEmpty())
        {
            s_libraryMPSSE->setFileName(mpsseDllPath);
            QByteArray logFileName = s_libraryMPSSE->fileName().toUtf8();
            LibWTM2100WB::print("FlashThread::FlashThread: load %s", logFileName.constData());
            QString mpsses1 = QString("FlashThread::FlashThread: load %1").arg(logFileName.constData());
            emit logMessage(mpsses1);
            ok = s_libraryMPSSE->load();
            if (!ok)
            {
                QByteArray logerr = s_libraryMPSSE->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
                QString mpssesError1 = QString("error: load %1 failed %2").arg(logFileName.constData()).arg(logerr.constData());
                emit logMessage(mpssesError1);
            }
        }
        if (!s_libraryMPSSE->isLoaded())
        {
            s_libraryMPSSE->setFileName("libMPSSE.dll");
            QByteArray logFileName = s_libraryMPSSE->fileName().toUtf8();
            LibWTM2100WB::print("FlashThread::FlashThread: load %s", logFileName.constData());
            QString mpsses2 = QString("FlashThread::FlashThread: load %1").arg(logFileName.constData());
            emit logMessage(mpsses2);
            ok = s_libraryMPSSE->load();
            if (!ok)
            {
                QByteArray logerr = s_libraryMPSSE->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
                QString mpssesError2 = QString("error: load %1 failed %2").arg(logFileName.constData()).arg(logerr.constData());
                emit logMessage(mpssesError2);
            }
        }
        if (!s_libraryMPSSE->isLoaded())
        {
            QDir dir = QCoreApplication::applicationDirPath();
            s_libraryMPSSE->setFileName(dir.absoluteFilePath("libMPSSE.dll"));
            QByteArray logFileName = s_libraryMPSSE->fileName().toUtf8();
            LibWTM2100WB::print("FlashThread::FlashThread: load %s", logFileName.constData());
            QString mpsses3 = QString("FlashThread::FlashThread: load %1").arg(logFileName.constData());
            emit logMessage(mpsses3);
            ok = s_libraryMPSSE->load();
            if (!ok)
            {
                QByteArray logerr = s_libraryMPSSE->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
                QString mpssesError3 = QString("error: load %1 failed %2").arg(logFileName.constData()).arg(logerr.constData());
                emit logMessage(mpssesError3);
                return;
            }
        }

        GetSPIfuncs(s_libraryMPSSE);
        libtunnelInit(s_libraryTunnel);
        boolInitLibFuncs();
        // uint32_t readyArray[MAX_SITE_SIZE_FLASH] = {1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        //                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        // setReadyArr(readyArray);
        
    }
}

void FlashThread::libtunnelInit(QLibrary *libtunnel)
{
    if (libtunnel == nullptr)
    {
        QString tunnelDllPath = Settings::staticInstance()->value("tunnelDllPath").toString();
        bool ok = false;
        libtunnel = new QLibrary();
        if (!tunnelDllPath.isEmpty())
        {
            libtunnel->setFileName(tunnelDllPath);
            QByteArray logFileName = libtunnel->fileName().toUtf8();
            LibWTM2100WB::print("UartThread::UartThread: load %s", logFileName.constData());
            QString tunnels1 = QString("UartThread::FlashThread: load %1").arg(logFileName.constData());
            emit logMessage(tunnels1);
            ok = libtunnel->load();
            if (!ok)
            {
                QByteArray logerr = libtunnel->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
                QString tunnelsError1 = QString("error: load %1 failed %2").arg(logFileName.constData()).arg(logerr.constData());
                emit logMessage(tunnelsError1);
            }
        }
        if (!libtunnel->isLoaded())
        {
            libtunnel->setFileName("libTunnel_64.dll");
            QByteArray logFileName = libtunnel->fileName().toUtf8();
            LibWTM2100WB::print("UartThread::UartThread: load %s", logFileName.constData());
            QString tunnels2 = QString("UartThread::FlashThread: load %1").arg(logFileName.constData());
            emit logMessage(tunnels2);
            ok = libtunnel->load();
            if (!ok)
            {
                QByteArray logerr = libtunnel->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
                QString tunnelsError2 = QString("error: load %1 failed %2").arg(logFileName.constData()).arg(logerr.constData());
                emit logMessage(tunnelsError2);
            }
        }
        if (!libtunnel->isLoaded())
        {
            QDir dir = QCoreApplication::applicationDirPath();
            libtunnel->setFileName(dir.absoluteFilePath("libTunnel_64.dll"));
            QByteArray logFileName = libtunnel->fileName().toUtf8();
            LibWTM2100WB::print("UartThread::UartThread: load %s", logFileName.constData());
            QString tunnels3 = QString("UartThread::FlashThread: load %1").arg(logFileName.constData());
            emit logMessage(tunnels3);
            ok = libtunnel->load();
            if (!ok)
            {
                QByteArray logerr = libtunnel->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
                QString tunnelsError3 = QString("error: load %1 failed %2").arg(logFileName.constData()).arg(logerr.constData());
                emit logMessage(tunnelsError3);
                return;
            }
        }
        set_ready_arrFlash = reinterpret_cast<TunnelFuncFlash>(libtunnel->resolve("set_ready_arr"));
        get_ready_arrFlash = reinterpret_cast<TunnelFuncFlash>(libtunnel->resolve("get_ready_arr"));
    }
    memset(m_Arr, 0, sizeof(m_Arr));
    get_ready_arrFlash(m_Arr);
}

void FlashThread::setReadyArr(const uint32_t *ReadyArray)
{
    memcpy(m_Arr, ReadyArray, sizeof(m_Arr));
    set_ready_arrFlash(m_Arr);
}

void FlashThread::GetSPIfuncs(QLibrary *libmpsse)
{
    Init_libMPSSE = reinterpret_cast<Init_libMPSSE_ptr>(libmpsse->resolve("Init_libMPSSE"));
    Cleanup_libMPSSE = reinterpret_cast<Cleanup_libMPSSE_ptr>(libmpsse->resolve("Cleanup_libMPSSE"));
    SPI_GetNumChannels = reinterpret_cast<SPI_GetNumChannels_ptr>(libmpsse->resolve("SPI_GetNumChannels"));
    SPI_GetChannelInfo = reinterpret_cast<SPI_GetChannelInfo_ptr>(libmpsse->resolve("SPI_GetChannelInfo"));

    SPI_OpenChannel = reinterpret_cast<SPI_OpenChannel_ptr>(libmpsse->resolve("SPI_OpenChannel"));
    SPI_CloseChannel = reinterpret_cast<SPI_CloseChannel_ptr>(libmpsse->resolve("SPI_CloseChannel"));
    SPI_InitChannel = reinterpret_cast<SPI_InitChannel_ptr>(libmpsse->resolve("SPI_InitChannel"));
    FT_WriteGPIO = reinterpret_cast<FT_WriteGPIO_ptr>(libmpsse->resolve("FT_WriteGPIO"));

    FT_ReadGPIO = reinterpret_cast<FT_ReadGPIO_ptr>(libmpsse->resolve("FT_ReadGPIO"));
    Mid_SetGPIOLow = reinterpret_cast<Mid_SetGPIOLow_ptr>(libmpsse->resolve("Mid_SetGPIOLow"));
    SPI_Read = reinterpret_cast<SPI_Read_ptr>(libmpsse->resolve("SPI_Read"));
    SPI_Write = reinterpret_cast<SPI_Write_ptr>(libmpsse->resolve("SPI_Write"));
    Mid_CyclePort = reinterpret_cast<Mid_CyclePort_ptr>(libmpsse->resolve("Mid_CyclePort"));

    CHECK_NULL(Init_libMPSSE);
    CHECK_NULL(Cleanup_libMPSSE);
    CHECK_NULL(SPI_GetNumChannels);
    CHECK_NULL(SPI_GetChannelInfo);
    CHECK_NULL(SPI_OpenChannel);
    CHECK_NULL(SPI_CloseChannel);
    CHECK_NULL(SPI_InitChannel);
    CHECK_NULL(FT_WriteGPIO);
    CHECK_NULL(FT_ReadGPIO);
    CHECK_NULL(Mid_SetGPIOLow);
    CHECK_NULL(SPI_Read);
    CHECK_NULL(SPI_Write);
    CHECK_NULL(Mid_CyclePort);
}

void FlashThread::setSiteDir(const QDir &dir)
{
    m_dirSite = dir;
}

QDir FlashThread::getDirsite()
{
    return m_dirSite;
}

FlashThread::~FlashThread()
{
    wait();
    deleteLater();
}

void FlashThread::start(const QString &uart1, const QString &uart2)
{
    m_uart1Name = uart1;
    m_uart2Name = uart2;
    QThread::start();
}

void FlashThread::run()
{
}

bool FlashThread::openUart(const QString &uart1, const QString &uart2)
{
    LibWTM2100WB::print("Func::openUart()");
    m_uart1 = new QSerialPort;
    m_uart2 = new QSerialPort;

    bool ok = true;

    m_uart1->setPortName(uart1);
    m_uart1->setBaudRate(UART_BAUDRATE_FLASH);
    m_uart1->setDataBits(QSerialPort::Data8);
    m_uart1->setParity(QSerialPort::NoParity);
    m_uart1->setStopBits(QSerialPort::OneStop);
    m_uart1->setFlowControl(QSerialPort::NoFlowControl);
    ok = m_uart1->open(QIODevice::ReadWrite);
    if (!ok)
    {
        QByteArray logPort1Name = m_uart1->portName().toUtf8();
        QByteArray logerr = m_uart1->errorString().toUtf8();
        LibWTM2100WB::print("FlashThread::openUart: open %s failed. %s",
                            logPort1Name.constData(), logerr.constData());
        delete m_uart1;
        delete m_uart2;
        return false;
    }

    m_uart2->setPortName(uart2);
    m_uart2->setBaudRate(UART_BAUDRATE_FLASH);
    m_uart2->setDataBits(QSerialPort::Data8);
    m_uart2->setParity(QSerialPort::NoParity);
    m_uart2->setStopBits(QSerialPort::OneStop);
    m_uart2->setFlowControl(QSerialPort::NoFlowControl);
    ok = m_uart2->open(QIODevice::ReadWrite);
    if (!ok)
    {
        QByteArray logPort2Name = m_uart2->portName().toUtf8();
        QByteArray logerr = m_uart2->errorString().toUtf8();
        LibWTM2100WB::print("FlashThread::openUart: open %s failed. %s",
                            logPort2Name.constData(), logerr.constData());
        m_uart1->close();
        delete m_uart1;
        delete m_uart2;
        return false;
    }
    m_uart1->setRequestToSend(false);
    m_uart2->setRequestToSend(false);
    QThread::msleep(200);
    m_uart1->setRequestToSend(true);
    m_uart2->setRequestToSend(true);

    return true;
}

bool FlashThread::closeUart()
{
    LibWTM2100WB::print("Func::closeUart()");
    // bool ok = initSite(m_initArray, m_readyArray, 0);
    // if (!ok)
    // {
    //     LibWTM2100WB::print("UartThread::initFPGA: error: init sites to 0");
    // }
    m_uart1->setRequestToSend(false);
    m_uart2->setRequestToSend(false);
    QThread::msleep(1000);
    QByteArray logerr1 = m_uart1->errorString().toUtf8();
    QByteArray logerr2 = m_uart2->errorString().toUtf8();
    LibWTM2100WB::print("FlashThread::closeUart: %s %s",
                        logerr1.constData(), logerr2.constData());
    m_uart1->close();
    m_uart2->close();
    delete m_uart1;
    delete m_uart2;
    m_uart1 = nullptr;
    m_uart2 = nullptr;
    return true;
}

bool FlashThread::boolInitLibFuncs()
{
    return true;
}

void FlashThread::resetPort()
{
    Init_libMPSSE();

    FT_DEVICE_LIST_INFO_NODE devList;
    ChannelConfig config;

    config.ClockRate = 1000000;
    config.LatencyTimer= 255;
    config.configOptions = \
    SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;
    config.Pin = 0xFFFFFFFF;/*FinalVal-FinalDir-InitVal-InitDir (for dir 0=in, 1=out)*/

    DWORD numChannels;
    unsigned int  status = SPI_GetNumChannels(&numChannels);
    if (status != 0) {
        QString message = QString("Error getting the number of SPI channels.");
        emit logMessage(message);
        LibWTM2100WB::print("Error getting the number of SPI channels.");
        Cleanup_libMPSSE();
        return;
    }

    if (numChannels == 0) {
        return;
    }

    status = SPI_OpenChannel(0, &handlea);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error opening SPI channel.");
        QString opens1 = QString("Error opening SPI channel.");
        emit logMessage(opens1);
        Cleanup_libMPSSE();
        return;
    }

    FT_HANDLE handleb;
    status = SPI_OpenChannel(1, &handleb);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error opening SPI channel.");
        QString opens2 = QString("Error opening SPI channel.");
        emit logMessage(opens2);
        Cleanup_libMPSSE();
        return;
    }

    status = SPI_InitChannel(handlea, &config);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error initializing channel.");
        QString inits1 = QString("Error initializing channel.");
        emit logMessage(inits1);
        Cleanup_libMPSSE();
        return;
    }

    ChannelConfig configbd;
    configbd.ClockRate = 7500000;
    configbd.LatencyTimer= 255;
    configbd.configOptions = \
    SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;
    
    configbd.Pin = 0xFFFFFFFF;/*FinalVal-FinalDir-InitVal-InitDir (for dir 0=in, 1=out)*/
    status = SPI_InitChannel(handleb, &configbd);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error initializing channel.");
        QString inits2 = QString("Error initializing channel.");
        emit logMessage(inits2);
        Cleanup_libMPSSE();
        return;
    }

    // reset gpio
    Mid_CyclePort(handlea);
    Mid_CyclePort(handleb);
    this_thread::sleep_for(chrono::milliseconds(100));
    LibWTM2100WB::print("Reset ft2232 GPIOs.");
    QString resetPins = QString("Reset ft2232 GPIOs.\n");
    emit logMessage(resetPins);
    SPI_CloseChannel(handlea);
    SPI_CloseChannel(handleb);
    Cleanup_libMPSSE();
}

bool FlashThread::configSPI(int pin1, int pin2, const QString &path)
{
    Init_libMPSSE();

    FT_DEVICE_LIST_INFO_NODE devList;
    ChannelConfig config;

    config.ClockRate = 1000000;
    config.LatencyTimer= 255;
    config.configOptions = \
    SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;
    config.Pin = 0xFFFFFFFF;/*FinalVal-FinalDir-InitVal-InitDir (for dir 0=in, 1=out)*/

    DWORD numChannels;
    unsigned int  status = SPI_GetNumChannels(&numChannels);
    if (status != 0) {
        QString message = QString("Error getting the number of SPI channels.");
        emit logMessage(message);
        emit failedSignal(true);
        // emit manuFinish(true);
        LibWTM2100WB::print("Error getting the number of SPI channels.");
        Cleanup_libMPSSE();
    }
    LibWTM2100WB::print("Number of SPI channels: %d", numChannels);
    QString numChannel = QString("Number of SPI channels: %1").arg(numChannels);
    emit logMessage(numChannel);

    if (numChannels == 0)
    {
        emit failedSignal(true);
        // emit manuFinish(true);
        return false;
    }
    if (numChannels > 0) {
        for (int i = 0; i < numChannels; i++) {
            status = SPI_GetChannelInfo(i, &devList);
            LibWTM2100WB::print("Information on channel number %d :", i);
            QString iii = QString("Information on channel number %1: ").arg(i);
            emit logMessage(iii);
            /* print the dev info */
            LibWTM2100WB::print("  Flags=0x%x", devList.Flags);
            QString Flagss = QString("  Flags=0x%1").arg(devList.Flags, 0, 16);
            emit logMessage(Flagss);
            LibWTM2100WB::print("  Type=0x%x", devList.Type);
            QString Types = QString("  Type=0x%1").arg(devList.Type, 0, 16);
            emit logMessage(Types);
            LibWTM2100WB::print("  ID=0x%x", devList.ID);
            QString IDs = QString("  ID=0x%1").arg(devList.ID, 0, 16);
            emit logMessage(IDs);
            LibWTM2100WB::print("  LocId=0x%x", devList.LocId);
            QString LocIds = QString("  LocId=0x%1").arg(devList.LocId, 0, 16);
            emit logMessage(LocIds);
            LibWTM2100WB::print("  SerialNumber=%s", devList.SerialNumber);
            QString SerialNumbers = QString("  SerialNumber=0x%1").arg(devList.SerialNumber, 0, 16);
            emit logMessage(SerialNumbers);
            LibWTM2100WB::print("  Description=%s", devList.Description);
            QString Descriptions = QString("  Description=0x%1").arg(devList.Description, 0, 16);
            emit logMessage(Descriptions);
            LibWTM2100WB::print("  ftHandle=0x%x", devList.ftHandle);
            QString ftHandles = QString("  ftHandle=0x%1").arg(0, 0, 16);
            emit logMessage(ftHandles);
        }
    }

    status = SPI_OpenChannel(0, &handlea); // 假设选择第1个SPI通道
    if (status != FT_OK) {
        LibWTM2100WB::print("Error opening SPI channel.");
        QString opens1 = QString("Error opening SPI channel.");
        emit logMessage(opens1);
        emit failedSignal(true);
        // emit manuFinish(true);
        Cleanup_libMPSSE();
        return false;
    }

    FT_HANDLE handleb;
    status = SPI_OpenChannel(1, &handleb); // 假设选择第2个SPI通道
    if (status != FT_OK) {
        LibWTM2100WB::print("Error opening SPI channel.");
        QString opens2 = QString("Error opening SPI channel.");
        emit logMessage(opens2);
        emit failedSignal(true);
        // emit manuFinish(true);
        Cleanup_libMPSSE();
        return false;
    }

    status = SPI_InitChannel(handlea, &config);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error initializing channel.");
        QString inits1 = QString("Error initializing channel.");
        emit logMessage(inits1);
        emit failedSignal(true);
        // emit manuFinish(true);
        Cleanup_libMPSSE();
        Q_EMIT completed();
        return false;
    }

    ChannelConfig configbd;
    configbd.ClockRate = 7500000;
    configbd.LatencyTimer= 255;
    configbd.configOptions = \
    SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;
    
    configbd.Pin = 0xFFFFFFFF;/*FinalVal-FinalDir-InitVal-InitDir (for dir 0=in, 1=out)*/
    status = SPI_InitChannel(handleb, &configbd);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error initializing channel.");
        QString inits2 = QString("Error initializing channel.");
        emit logMessage(inits2);
        emit failedSignal(true);
        // emit manuFinish(true);
        Cleanup_libMPSSE();
        Q_EMIT completed();
        return false;
    }

    // 设置AC0、AC1
    uint8_t dir = 0xFF;
    uint8_t value = 0xFF;

    // clr ac1
    // pin1 = 1;
    uint16_t val_a = 0xFFFF;
    uint16_t val_b = 0xFFFF;
    val_a &= ~(1<<(pin1+8));
    Mid_SetGPIOLow(handlea, val_a&0xff, dir);
    Mid_SetGPIOLow(handleb, val_b&0xff, dir);
    FT_WriteGPIO(handlea, dir, val_a>>8);
    FT_WriteGPIO(handleb, dir, val_b>>8);
    this_thread::sleep_for(chrono::milliseconds(500));
    QString spimodes = QString("clear ac1. Now ac1 is low level and we are in SPI Flash W/E mode. ");
    emit logMessage(spimodes);


    // erase flash[i]
    LibWTM2100WB::print("Erase flash%d\r\n", pin2+1);
    QString erasePins0 = QString("\n①Erase flash%1.\nEras...").arg(pin2+1);
    emit logMessage(erasePins0);
    QString erasePinsIng0 = QString("···");
    emit logMessage(erasePinsIng0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    w25qxx_erase_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    // test flash[i]
    LibWTM2100WB::print("Check flash%d is erased correctly.\r\n", pin2+1);
    QString readAgainPins0 = QString("Check flash%1 is erased correctly.").arg(pin2+1);
    emit logMessage(readAgainPins0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    uint8_t regErase = w25qxx_read_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    QString regErase0 = QString("0x%1").arg(regErase, 0, 16);
    if (regErase0 != "0xff")
    {
        LibWTM2100WB::print("flash%d erase failed!\r\n", pin2+1);
        emit logMessage(QString("flash%1 erase failed!").arg(pin2+1));
        emit failedSignal(true);
        // emit manuFinish(true);
        // release
        SPI_CloseChannel(handlea);
        SPI_CloseChannel(handleb);
        Cleanup_libMPSSE();
        LibWTM2100WB::print("End.");
        QString writePinsEnd0 = QString("End.\n");
        emit logMessage(writePinsEnd0);
        return false;
    }
    else
    {
        emit failedSignal(false);
        LibWTM2100WB::print("flash%d erase successfully!\r\n", pin2+1);
        emit logMessage(QString("flash%1 erase successfully!").arg(pin2+1));
    }


    // write flash[i]
    LibWTM2100WB::print("Program flash%d\r\n", pin2+1);
    QString writePins0 = QString("\n②Program flash%1.\nProgramming...").arg(pin2+1);
    emit logMessage(writePins0);
    QString writePinsIng0 = QString("···");
    emit logMessage(writePinsIng0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    w25qxx_write_byte(handleb, path);
    this_thread::sleep_for(chrono::milliseconds(2000));
    // test flash[i]
    LibWTM2100WB::print("Check flash%d is programmed correctly.\r\n", pin2+1);
    QString writeTest0 = QString("Check flash%1 is programmed correctly.").arg(pin2+1);
    emit logMessage(writeTest0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    uint8_t regWrite = w25qxx_read_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    QString regWrite0 = QString("0x%1").arg(regWrite, 0, 16);
    if (regWrite0 == "0xff")
    {
        LibWTM2100WB::print("flash%d program failed!\r\n", pin2+1);
        emit logMessage(QString("flash%1 program failed!").arg(pin2+1));
        emit failedSignal(true);
        // emit manuFinish(true);
        // 释放资源
        SPI_CloseChannel(handlea);
        SPI_CloseChannel(handleb);
        Cleanup_libMPSSE();
        LibWTM2100WB::print("End.");
        QString writePinsEnd0 = QString("End.\n");
        emit logMessage(writePinsEnd0);
        return false;
    }
    else
    {
        emit failedSignal(false);
        LibWTM2100WB::print("flash%d program successfully!\r\n", pin2+1);
        emit logMessage(QString("flash%1 program successfully!").arg(pin2+1));
    }


    // verify flash[i]
    LibWTM2100WB::print("Check flash%d\r\n", pin2+1);
    QString verifyPins0 = QString("\n③Check flash%1.\nChecking...").arg(pin2+1);
    emit logMessage(verifyPins0);
    QString verifyPinsIng0 = QString("···");
    emit logMessage(verifyPinsIng0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    bool is_successEW = w25qxx_verify_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    if (!is_successEW)
    {
        LibWTM2100WB::print("flash%d check failed!\r\n", pin2+1);
        emit logMessage(QString("flash%1 check failed!").arg(pin2+1));
        emit failedSignal(true);
        // emit manuFinish(true);
        // release
        SPI_CloseChannel(handlea);
        SPI_CloseChannel(handleb);
        Cleanup_libMPSSE();
        LibWTM2100WB::print("End.");
        QString writePinsEnd0 = QString("End.\n");
        emit logMessage(writePinsEnd0);
        return false;
    }
    else
    {
        emit failedSignal(false);
        LibWTM2100WB::print("flash%d check successfully!\r\n", pin2+1);
        emit logMessage(QString("flash%1 check successfully!").arg(pin2+1));
    }

    // close SPI channels
    SPI_CloseChannel(handlea);
    SPI_CloseChannel(handleb);
    Cleanup_libMPSSE();
    LibWTM2100WB::print("End.");
    QString writePinsEnd0 = QString("End.\n");
    emit logMessage(writePinsEnd0);
    // emit manuFinish(true);

    return true;
}

uint8_t FlashThread::w25qxx_write_byte(FT_HANDLE handle, const QString &filePath)
{
    uint8_t id = 0x00, temp = 0x06, disableWrite = 0x04, erase = 0x20;

    uint8_t readbuffer[2] ={0};
    uint8_t temp3 = 0;
    uint32_t sizeTransferred;
    // Write Enable
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QString writes1 = QString("Failed to open the <firmware>.bin file for reading");
        emit logMessage(writes1);
        emit failedSignal(true);
        LibWTM2100WB::print("Failed to open the .bin file for reading");
        return 0;
    }

    m_Bindata = file.readAll();
    file.close();
    LibWTM2100WB::print("size: %d", m_Bindata.size());

    uint8_t pageBuffer2[m_Bindata.size()] = {0};
    uint8_t fakeBuffer[2] = {0x37, 0x5};

    /* Write page*/
    uint8_t tempEnable = 0x02;

    // [0~2] means bits, [3~4] means the data writing to page
    uint8_t tempBufferBit[3] = {0};
    tempBufferBit[0] = (id >> 16) & 0xff;
    tempBufferBit[1] = (id >> 8) & 0xff;
    tempBufferBit[2] = 0x00;

    uint32_t pageSize = 256;
    uint32_t pageCount = (m_Bindata.size() + pageSize - 1) / pageSize;
    // std::ofstream outputFile("outputpin22.txt", std::ios_base::app);
    for (int i = 0; i < pageCount; i++) {
        uint8_t pageBuffer3[pageSize] = {0};
        uint32_t blockSize = qMin(pageSize, m_Bindata.size() - i * pageSize);

        // copy blocksize's length to buffer3
        memcpy(pageBuffer3, m_Bindata.data() + i * pageSize, blockSize);
        // Write Enable
        SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE
                                                    |SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // write data eanble
        SPI_Write(handle, &tempEnable, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
        // bits high/low
        SPI_Write(handle, tempBufferBit, sizeof(tempBufferBit), &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
        // write page3 to spi slave
        SPI_Write(handle, pageBuffer3, blockSize, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES
                                        | SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE | SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // disable write -- 如果不加这行代码，write会跳页而写，即会漏掉一些page
        SPI_Write(handle, &disableWrite, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE
                                                    |SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);

        // if (outputFile.is_open())
        // {
        //     for (int j = 0; j < 256; j++)
        //     {outputFile << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(pageBuffer3[j])) << std::endl;}}
            
        // else{LibWTM2100WB::print("Error open readputpin22.txt");}

        // page++, sector++, block++
        tempBufferBit[1]++;
        if (tempBufferBit[1] == 0) {
            LibWTM2100WB::print("-------------------------spliter--------------------");
            tempBufferBit[0]++;
        }
    }

    // QString writeSuccess0 = QString("We write 0x%1  0x%2 to the flash.\nNow we read again").arg(fakeBuffer[0], 0, 16).arg(fakeBuffer[1], 0, 16);
    // emit logMessage(writeSuccess0);

    return pageBuffer2[0];
}

uint8_t FlashThread::w25qxx_read_byte(FT_HANDLE handle)
{
    uint8_t id = 0x00, temp = 0x06, disableWrite = 0x04, erase = 0xd8/*20*/;

    uint8_t readbuffer[2] ={0x00};
    uint8_t temp3 = 0;
    uint8_t tempeanble = 0x03;
    uint32_t sizeTransferred;
    
    uint8_t tempBufferBit[3] = {0};
    tempBufferBit[0] = (id >> 16) & 0xff;
    tempBufferBit[1] = (id >> 8) & 0xff;
    tempBufferBit[2] = 0x00;
    uint32_t pageCount = 2107;
    std::ofstream outputFile("readputpin21-erase.txt", std::ios_base::app);
    for (int i = 0; i < 1; i++)
    {
        uint8_t readbuffer3[256] ={0};
        uint32_t blockSize = qMin(256, 539272 - i * 256);
        // Write Enable
        SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE|SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // read data eanble
        SPI_Write(handle, &tempeanble, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
        // bits high/low
        SPI_Write(handle, tempBufferBit, 3, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
        // read to buffer3
        SPI_Read(handle, readbuffer3, blockSize, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES
                                        | SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE | SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // SPI_Write(handle, &disableWrite, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE
        //                                             |SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // output to a file
        if (outputFile.is_open())
        {
            if(blockSize == 256)
            {
                for (int j = 0; j < 256; j++)
                {outputFile << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(readbuffer3[j])) << std::endl;}
            }
            else
            {
                for (int j = 0; j < 136; j++)
                {outputFile << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(readbuffer3[j])) << std::endl;}
            }
        }
        else{LibWTM2100WB::print("Error open readputpin21-erase.txt");}
        // to return
        readbuffer[0] = readbuffer3[0];

        // page++, sector++, block++
        tempBufferBit[1]++;
        if (tempBufferBit[1] == 0) {
            LibWTM2100WB::print("-------------------------spliter--------------------");
            tempBufferBit[0]++;
        }
    }

    // LibWTM2100WB::print("read Register:0x%x  0x%x", readbuffer2[0], readbuffer2[1]);
    // QString reads0 = QString("read Register:0x%1  0x%2").arg(readbuffer2[0], 0, 16).arg(readbuffer2[1], 0, 16);
    // emit logMessage(reads0);

    return readbuffer[0];
}

uint8_t FlashThread::w25qxx_erase_byte(FT_HANDLE handle)
{
    uint8_t id = 0x00, temp = 0x06, disableWrite = 0x04, erase = 0xc7;

    uint8_t readbuffer[2] ={0};
    uint8_t temp3 = 0;
    uint32_t sizeTransferred;

    //bits
    uint8_t eraseBits[3] = {0};
    eraseBits[0] = (id>>16) & 0xff;
    eraseBits[1] = (id>>8) & 0xff;
    eraseBits[2] = (id) & 0xff;

    for (int i = 0; i < 1; i++)
    {
        // Write Enable
        SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE
                                                    |SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // erase enable
        SPI_Write(handle, &erase, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE
                                                 |SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // SPI_Write(handle, eraseBits, sizeof(eraseBits), &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES
        //                                             |SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
    }
    this_thread::sleep_for(chrono::milliseconds(40000));
    return id;
}

bool FlashThread::w25qxx_verify_byte(FT_HANDLE handle)
{
    uint8_t id = 0x00, disableWrite = 0x04, temp = 0x06/*20*/;

    uint8_t tempeanble = 0x03;
    // uint8_t readbuffer[256] ={0};
    uint32_t sizeTransferred;

    uint32_t pageSize = 256;
    uint8_t tempBufferBit[3] = {0};
    tempBufferBit[0] = (id >> 16) & 0xff;
    tempBufferBit[1] = (id >> 8) & 0xff;
    tempBufferBit[2] = 0x00;
    uint32_t pageCount = (m_Bindata.size() + pageSize - 1) / pageSize;
    if(pageCount < 1 )
    {
        return false;
    }
    for (int i = 0; i < pageCount; i++)
    {
        uint8_t readbuffer[256] ={0};
        uint8_t pageBuffer[pageSize] = {0};
        uint32_t blockSize = qMin(pageSize, m_Bindata.size() - i * pageSize);
        // copy blockSize's length data from m_Bindata to pageBuffer. 
        memcpy(pageBuffer, m_Bindata.data() + i * pageSize, blockSize);
        // Write Enable
        SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE|SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // read data eanble
        SPI_Write(handle, &tempeanble, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
        // [0~2] means bits, readbuffer means the data read from page
        SPI_Write(handle, tempBufferBit, 3, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
        // read to buffer
        SPI_Read(handle, readbuffer, blockSize, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES
                                        | SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE | SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
        // SPI_Write(handle, &disableWrite, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE
        //                                             |SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);

        // verify the bin's srcData and the gotten data
        for(int j = 0; j < 256; j++)
        {
            if(readbuffer[j] != pageBuffer[j])
            {
                LibWTM2100WB::print("Check failed-reason: The number of%d failed", i*256+j+1);
                LibWTM2100WB::print("srcData:0x%x, readData:0x%x", pageBuffer[j], readbuffer[j]);
                QString verifyData0 = QString("Check failed-reason: The number of%1 failed").arg(i*256+j+1);
                emit logMessage(verifyData0);

                return false;
            }
        }  

        // page++, sector++, block++
        tempBufferBit[1]++;
        if (tempBufferBit[1] == 0) {
            tempBufferBit[0]++;
        }
    }
    return true;
}