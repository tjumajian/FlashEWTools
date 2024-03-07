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
//    int i = 0;
//    for (; i < MAX_SITE_SIZE; i++)
//    {
//        s_ReadyArray[i] = 1;
//    }
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

QList<int> FlashThread::getSelectedAddresses(const uint32_t* readyArray, int arraySize)
{
    QList<int> selectedAddresses;
    for (int i = 0; i < arraySize; i++)
    {
        if (readyArray[i] == 1)
        {
            selectedAddresses.append(i);
        }
    }
    return selectedAddresses;
}

uint32_t FlashThread::getHexAddress(int address)
{
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(8) << std::hex << ((static_cast<uint32_t>(address) <<24) | 0x00FF00FF);
    
    uint32_t result;
    ss >> std::hex >> result;
    
    return result;
}

uint32_t FlashThread::getHexAddressAD(int address)
{
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(8) << std::hex << ((static_cast<uint32_t>(address) <<24) | 0xEFFB00FB);
    
    uint32_t result;
    ss >> std::hex >> result;
    
    return result;
}

uint32_t FlashThread::getSiteHex(int site)
{
    std::stringstream ss;
    ss << std::hex << site;
    uint32_t result;
    ss >> result;
    return result;
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
    wait(); // 等待线程执行完毕
    deleteLater(); // 延迟删除线程对象
}

void FlashThread::start(const QString &uart1, const QString &uart2)
{
    m_uart1Name = uart1;
    m_uart2Name = uart2;
    QThread::start();
}

void FlashThread::run()
{
    // bool ok = openUart(m_uart1Name, m_uart2Name);
    // if (!ok)
    // {
    //     Q_EMIT completed();
    //     return;
    // }
    // closeUart();
    Init_libMPSSE();
    SPIcommunicate();

    Q_EMIT completed();
}

bool FlashThread::openUartNoParam()
{
    openUart(m_uart1Name, m_uart2Name);
    return true;
}

bool FlashThread::boolInitLibFuncs()
{
    return true;
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

FT_STATUS FlashThread::read_byte(uint8_t slaveAddress, uint8_t address, uint16_t *data)
{

}

FT_STATUS FlashThread::write_byte(uint8_t slaveAddress, uint8_t address, uint16_t data)
{

}

ChannelConfig* FlashThread::SPIconfigAD()
{

}

ChannelConfig* FlashThread::SPIconfigBD()
{

}

// void FlashThread::initLibMPSSE()
// {
//     LibWTM2100WB::print("T");
//     QString message = QString("T");
//     emit logMessage(message);
// }

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

    status = SPI_OpenChannel(0, &handlea); // 假设选择第1个SPI通道
    if (status != FT_OK) {
        LibWTM2100WB::print("Error opening SPI channel.");
        QString opens1 = QString("Error opening SPI channel.");
        emit logMessage(opens1);
        Cleanup_libMPSSE();
        return;
    }

    FT_HANDLE handleb;
    status = SPI_OpenChannel(1, &handleb); // 假设选择第2个SPI通道
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
    uint8_t dir = 0xFF; // 方向，每一位代表一个引脚，1表示输出，0表示输入
    uint8_t value = 0xFF; // 输出值，每一位代表一个引脚的输出值

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
    LibWTM2100WB::print("擦除flash%d\r\n", pin2+1);
    QString erasePins0 = QString("\n①擦除flash%1.\n擦除中...").arg(pin2+1);
    emit logMessage(erasePins0);
    QString erasePinsIng0 = QString("···");
    emit logMessage(erasePinsIng0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    w25qxx_erase_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    // test flash[i]
    LibWTM2100WB::print("检测flash%d 是否正确擦除\r\n", pin2+1);
    QString readAgainPins0 = QString("检测flash%1 是否正确擦除.").arg(pin2+1);
    emit logMessage(readAgainPins0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    uint8_t regErase = w25qxx_read_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    QString regErase0 = QString("0x%1").arg(regErase, 0, 16);
    if (regErase0 != "0xff")
    {
        LibWTM2100WB::print("flash%d 擦除失败!\r\n", pin2+1);
        emit logMessage(QString("flash%1 擦除失败!").arg(pin2+1));
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
        LibWTM2100WB::print("flash%d 擦除成功!\r\n", pin2+1);
        emit logMessage(QString("flash%1 擦除成功!").arg(pin2+1));
    }


    // write flash[i]
    LibWTM2100WB::print("编写flash%d\r\n", pin2+1);
    QString writePins0 = QString("\n②编写flash%1.\n编写中...").arg(pin2+1);
    emit logMessage(writePins0);
    QString writePinsIng0 = QString("···");
    emit logMessage(writePinsIng0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    w25qxx_write_byte(handleb, path);
    this_thread::sleep_for(chrono::milliseconds(2000));
    // test flash[i]
    LibWTM2100WB::print("检测flash%d 是否正确编写\r\n", pin2+1);
    QString writeTest0 = QString("检测flash%1 是否正确编写.").arg(pin2+1);
    emit logMessage(writeTest0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    uint8_t regWrite = w25qxx_read_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    QString regWrite0 = QString("0x%1").arg(regWrite, 0, 16);
    if (regWrite0 == "0xff")
    {
        LibWTM2100WB::print("flash%d 编写失败!\r\n", pin2+1);
        emit logMessage(QString("flash%1 编写失败!").arg(pin2+1));
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
        LibWTM2100WB::print("flash%d 编写成功!\r\n", pin2+1);
        emit logMessage(QString("flash%1 编写成功!").arg(pin2+1));
    }


    // verify flash[i]
    LibWTM2100WB::print("校验flash%d\r\n", pin2+1);
    QString verifyPins0 = QString("\n③校验flash%1.\n检验中...").arg(pin2+1);
    emit logMessage(verifyPins0);
    QString verifyPinsIng0 = QString("···");
    emit logMessage(verifyPinsIng0);
    Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    this_thread::sleep_for(chrono::milliseconds(100));
    bool is_successEW = w25qxx_verify_byte(handleb);
    this_thread::sleep_for(chrono::milliseconds(2000));
    if (!is_successEW)
    {
        LibWTM2100WB::print("flash%d 检验失败!\r\n", pin2+1);
        emit logMessage(QString("flash%1 检验失败!").arg(pin2+1));
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
        LibWTM2100WB::print("flash%d 检验成功!\r\n", pin2+1);
        emit logMessage(QString("flash%1 检验成功!").arg(pin2+1));
    }

    
    // // read again
    // LibWTM2100WB::print("Read FLASH pin%d\r\n", pin2);
    // QString readTriplePins0 = QString("Read FLASH pin%1 again.").arg(pin2);
    // emit logMessage(readTriplePins0);
    // Mid_SetGPIOLow(handlea, 0x00, dir);
    // Mid_SetGPIOLow(handlea, pin2&0xff, dir);
    // this_thread::sleep_for(chrono::milliseconds(100));
    // uint8_t regAgain = w25qxx_read_byte(handleb);
    // QString regAgain0 = QString("0x%1").arg(regAgain, 0, 16);
    // if (regAgain0 == "0xff" || regAgain0 == "0x0")
    // {
    //     emit failedSignal(true);
    // }
    // this_thread::sleep_for(chrono::milliseconds(2000));

    // reset gpio
    // Mid_CyclePort(handlea);
    // Mid_CyclePort(handleb);
    // LibWTM2100WB::print("Reset ft2232 GPIOs.");
    // QString resetPins = QString("Reset ft2232 GPIOs.\n");
    // emit logMessage(resetPins);
    // 关闭SPI通道
    SPI_CloseChannel(handlea);
    SPI_CloseChannel(handleb);
    // 调用清理函数
    Cleanup_libMPSSE();
    // // 卸载DLL库
    // s_libraryMPSSE->unload();
    LibWTM2100WB::print("End.");
    QString writePinsEnd0 = QString("End.\n");
    emit logMessage(writePinsEnd0);
    // emit manuFinish(true);

    return true;
}

int FlashThread::SPIcommunicate()
{
    //FT_HANDLE handleb;

    FT_DEVICE_LIST_INFO_NODE devList;
    // 初始化通道配置
    ChannelConfig config;
    // 设置 config 的值，具体根据 libmpsse 的文档或定义进行设置
    config.ClockRate = 1000000;
    config.LatencyTimer= 255;
    config.configOptions = \
    SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;
    config.Pin = 0xFFFFFFFF;/*FinalVal-FinalDir-InitVal-InitDir (for dir 0=in, 1=out)*/

    DWORD numChannels;
    unsigned int  status = SPI_GetNumChannels(&numChannels);
    if (status != 0) {
        LibWTM2100WB::print("Error getting the number of SPI channels.");
        Cleanup_libMPSSE();
        s_libraryMPSSE->unload();
    }
    LibWTM2100WB::print("Number of SPI channels: %d", numChannels);

    if (numChannels > 0) {
        for (int i = 0; i < numChannels; i++) {
            status = SPI_GetChannelInfo(i, &devList);
            LibWTM2100WB::print("Information on channel number %d :", i);
            /* print the dev info */
            LibWTM2100WB::print("  Flags=0x%x", devList.Flags);
            LibWTM2100WB::print("  Type=0x%x", devList.Type);
            LibWTM2100WB::print("  ID=0x%x", devList.ID);
            LibWTM2100WB::print("  LocId=0x%x", devList.LocId);
            LibWTM2100WB::print("  SerialNumber=%s", devList.SerialNumber);
            LibWTM2100WB::print("  Description=%s", devList.Description);
            LibWTM2100WB::print("  ftHandle=0x%x", devList.ftHandle);
        }
    }

    // 以下是GPIO相关的调用示例
    status = SPI_OpenChannel(0, &handlea); // 假设选择第1个SPI通道
    if (status != FT_OK) {
        LibWTM2100WB::print("Error opening SPI channel.");
        Cleanup_libMPSSE();
        s_libraryMPSSE->unload();
        return 1;
    }

    FT_HANDLE handleb;
    status = SPI_OpenChannel(1, &handleb); // 假设选择第2个SPI通道
    if (status != FT_OK) {
        LibWTM2100WB::print("Error opening SPI channel.");
        Cleanup_libMPSSE();
        s_libraryMPSSE->unload();
        return 1;
    }

    status = SPI_InitChannel(handlea, &config);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error initializing channel.");
        Cleanup_libMPSSE();
        s_libraryMPSSE->unload();
        Q_EMIT completed();
        return 1;
    }

    ChannelConfig configbd;
    configbd.ClockRate = 5000;
    configbd.LatencyTimer= 255;
    configbd.configOptions = \
    SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;

    configbd.Pin = 0xFFFFFFFF;/*FinalVal-FinalDir-InitVal-InitDir (for dir 0=in, 1=out)*/
    status = SPI_InitChannel(handleb, &configbd);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error initializing channel.");
        Cleanup_libMPSSE();
        s_libraryMPSSE->unload();
        Q_EMIT completed();
        return 1;
    }

    // 设置AC0、AC1
    uint8_t dir = 0xFF; // 方向，每一位代表一个引脚，1表示输出，0表示输入
    uint8_t value = 0xFF; // 输出值，每一位代表一个引脚的输出值
    status = FT_WriteGPIO(handlea, dir, value);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error writing to GPIO.");
        SPI_CloseChannel(handlea);
        Cleanup_libMPSSE();
        s_libraryMPSSE->unload();
        return 1;
    }
    status = FT_WriteGPIO(handleb, dir, value);
    if (status != FT_OK) {
        LibWTM2100WB::print("Error writing to GPIO.\n");
        SPI_CloseChannel(handleb);
        Cleanup_libMPSSE();
        // FreeLibrary(hLib);
        return 1;
    }

    char command[50];
                
    char action[7];
    char port1 , port2;
    int pin;

    uint16_t val_a = 0xFFFF;
    uint16_t val_b = 0xFFFF;
    QString binPath;
    LibWTM2100WB::print("\nEnter 'help' to find out how to use");
    while (1) {
        LibWTM2100WB::print("Enter command (e.g. 'help', 'set ac0', 'clr ac0', 'readid fl0', ...): ");
        cin.getline(command, sizeof(command));

        // 移除换行符
        command[strcspn(command, "\n")] = '\0';

        // 退出循环的条件
        if (strcmp(command, "exit") == 0) {
            break;
        }
        if(strcmp(command, "help") == 0 ) {
            LibWTM2100WB::print("-----------------------------------------------------------");
            LibWTM2100WB::print("command                explanation");
            LibWTM2100WB::print("set ad5                means set pin ad5 high level");
            LibWTM2100WB::print("clr ad5                means set pin ad5 low level");
            LibWTM2100WB::print("rst ac0                means reset ft2232");
            LibWTM2100WB::print("exit                   means exit the command line");
            LibWTM2100WB::print("erase fl0              means erase flash0");
            LibWTM2100WB::print("write fl0              means write flash0");
            LibWTM2100WB::print("read fl0               means read flash0");
            LibWTM2100WB::print("readid fl0             means read the flash0 device ID");
            LibWTM2100WB::print("-----------------------------------------------------------");
        }
        
        int parsed = sscanf(command, "%s %c%c%d", action, &port1, &port2, &pin);

        if (parsed == 4) {
            if (strcmp(action, "set") == 0) {
                if(port1=='a'){
                    if(port2=='d'){
                        val_a |= (1<<pin);
                    }
                    if(port2=='c'){
                        val_a |= (1<<(pin+8));
                    }
                }
                if(port1=='b'){
                    if(port2=='d'){
                       val_b |= (1<<pin); 
                    }
                    if(port2=='c'){
                       val_b |= (1<<(pin+8)); 
                    }
                }
                Mid_SetGPIOLow(handlea, val_a&0xff, dir);
                Mid_SetGPIOLow(handleb, val_b&0xff, dir);
                FT_WriteGPIO(handlea, dir, val_a>>8);
                FT_WriteGPIO(handleb, dir, val_b>>8);
                LibWTM2100WB::print("Set %c%c%d high 0x%x 0x%x.\n", port1,port2, pin , val_a, val_b);
            } else if (strcmp(action, "clr") == 0) {
                if(port1=='a'){
                    if(port2=='d'){
                        val_a &=  ~(1<<pin);
                    }
                    if(port2=='c'){
                        val_a &= ~(1<<(pin+8));
                    }
                }
                if(port1=='b'){
                    if(port2=='d'){
                       val_b &= ~(1<<pin); 
                    }
                    if(port2=='c'){
                       val_b &= ~(1<<(pin+8)); 
                    }
                }

                Mid_SetGPIOLow(handlea, val_a&0xff, dir);
                Mid_SetGPIOLow(handleb, val_b&0xff, dir);
                FT_WriteGPIO(handlea, dir, val_a>>8);
                FT_WriteGPIO(handleb, dir, val_b>>8);
                LibWTM2100WB::print("Clear %c%c%d low 0x%x 0x%x.\n", port1,port2, pin, val_a, val_b);
            } 
            else if (strcmp(action, "rst") == 0) {
                Mid_CyclePort(handlea);
                Mid_CyclePort(handleb);
                LibWTM2100WB::print("Mid_CyclePort \r\n");
                break;
            }
            else if (strcmp(action, "erase") == 0 && port1 == 'f' && port2 == 'l') {
                LibWTM2100WB::print("Erase FLASH pin%d\r\n", pin);
                Mid_SetGPIOLow(handlea, pin&0xff, dir);

                this_thread::sleep_for(chrono::milliseconds(100));
                w25qxx_erase_byte(handleb);
                this_thread::sleep_for(chrono::milliseconds(2000));
            }
            else if (strcmp(action, "write") == 0 && port1 == 'f' && port2 == 'l') {
                LibWTM2100WB::print("Write FLASH pin%d\r\n", pin);
                Mid_SetGPIOLow(handlea, pin&0xff, dir);

                this_thread::sleep_for(chrono::milliseconds(100));
                w25qxx_write_byte(handleb, binPath);
                this_thread::sleep_for(chrono::milliseconds(2000));
            }
            else if (strcmp(action, "read") == 0 && port1 == 'f' && port2 == 'l') {
                LibWTM2100WB::print("read FLASH pin%d\r\n", pin);
                Mid_SetGPIOLow(handlea, pin&0xff, dir);

                this_thread::sleep_for(chrono::milliseconds(100));
                w25qxx_read_byte(handleb);
                this_thread::sleep_for(chrono::milliseconds(2000));
            }
            else if (strcmp(action, "readid" ) == 0 && port1 == 'f' && port2 == 'l') {
                LibWTM2100WB::print("readID FLASH pin%d\r\n", pin);
                Mid_SetGPIOLow(handlea, pin&0xff, dir);

                this_thread::sleep_for(chrono::milliseconds(100));
                w25qxx_read_id(handleb);
                this_thread::sleep_for(chrono::milliseconds(2000));
            }
            
            else {
                LibWTM2100WB::print("Invalid action: %s %c%c%d\n", action, port1, port2, pin);
            }
        }

        else if (parsed == 1 && strcmp(action, "help") == 0){
            
        }
        
        else {
            LibWTM2100WB::print("Invalid command: %s\n", command);
        }
    }

    // 关闭SPI通道
    SPI_CloseChannel(handlea);
    SPI_CloseChannel(handleb);

    // 调用清理函数
    Cleanup_libMPSSE();

    // 卸载DLL库
    s_libraryMPSSE->unload();
    LibWTM2100WB::print("End");

    return 0;
}

uint8_t FlashThread::w25qxx_read_id(FT_HANDLE handle)
{
    //#if 0
                // Manufacturer/Device ID
                // 查表8.1.2 先write(0x90), 然后write(两个随机值), ，再write(0x00), 最后read()一次
                uint8_t id[2] = {0};
                // SPI_Write(FT_HANDLE handle, uint8_t *buffer, uint32_t sizeToTransfer, uint32_t *sizeTransferred, uint32_t transferOptions)
            //   SPI_ReadWrite(FT_HANDLE handle, uint8_t *inBuffer, uint8_t *outBuffer, uint32_t sizeToTransfer, uint32_t *sizeTransferred, uint32_t transferOptions)   
                //发送0x90，读取厂商ID和设备ID
                uint8_t temp = 0x90;
                uint32_t sizeTransferred;
                SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
                
                //发送24位地址(3个字节)  前面两个字节可以任意，第三个字节必须是0x00
                // spi_read_writeByte(0x00);
                // spi_read_writeByte(0x00);
                temp = 0x00;
                SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
                SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
                // spi_read_writeByte(0x00);//一定是0x00
                temp = 0x00;
                SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
                
                // id |= spi_read_writeByte(0xFF)<<8; //id：0xEF16  厂商ID：0xEF    
                // id |= spi_read_writeByte(0xFF);    //设备ID：0x16  

                // SPI_ReadWrite(handleb, &temp, &id, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
                SPI_Read(handle, id, 2, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
                LibWTM2100WB::print("ID0:0x%x  ", id[0]);
                // SPI_ReadWrite(handleb, &temp, &id, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
                // SPI_Read(handle, id, 2, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
                LibWTM2100WB::print("ID1:0x%x \r\n", id[1]);
    //#endif
    #if 0
            // JEDEC ID
            // 查表8.1.2 先wirte(0x9F), 然后read()三次
            uint8_t id = 0;
            uint8_t temp = 0x9f;
            uint32_t sizeTransferred;
            SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);

            SPI_Read(handle, &id, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
            printf("byte2:0x%x  ", id);
            SPI_Read(handle, &id, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
            printf("byte3:0x%x  ", id);
            SPI_Read(handle, &id, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
            printf("byte4:0x%x \r\n", id);
    #endif
    #if 0 
            // Release Power-down/ID
            // 查表8.1.2 先write(0xAB), 然后write(三个随机值), 最后read()一次
            uint8_t id = 0;
            uint8_t temp = 0xAB;
            uint32_32 sizeTransferred;
            SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);

            temp = 0x55;
            SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
            SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);
            SPI_Write(handle, &temp, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE);

            SPI_Read(handle, &id, 1, &sizeTransferred, SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);
            printf("byte4:0x%x \r\n", id);
    #endif
            
    return id[1];
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
                LibWTM2100WB::print("校验错误-错误原因: 第%d个字节错误", i*256+j+1);
                LibWTM2100WB::print("srcData:0x%x, readData:0x%x", pageBuffer[j], readbuffer[j]);
                QString verifyData0 = QString("校验错误-错误原因: 第%1个字节错误").arg(i*256+j+1);
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