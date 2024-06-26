#ifndef FLASHTHREAD_H
#define FLASHTHREAD_H

#include <QDir>
#include <QThread>
#include <QSerialPort>
#include <QLibrary>
#include "libMPSSE_spi.h"

enum
{
    MAX_SITE_SIZE_FLASH = 64,
    UART_BAUDRATE_FLASH = 115200
};

class FlashThread : public QThread
{
    Q_OBJECT
public:
    explicit FlashThread(QObject *parent = nullptr);
    virtual ~FlashThread();
    void start(const QString &uart1, const QString &uart2);
    void GetSPIfuncs(QLibrary *libmpsse);
    void libtunnelInit(QLibrary *libtunnel);
    bool boolInitLibFuncs();
    ChannelConfig* SPIconfigAD();
    ChannelConfig* SPIconfigBD();
    bool openUart(const QString &uart1, const QString &uart2);
    bool openUartNoParam();
    bool closeUart();
    int SPIcommunicate();
    void setSiteDir(const QDir &dir);
    QDir getDirsite();
    void setReadyArr(const uint32_t *ReadyArray);
    QList<int> getSelectedAddresses(const uint32_t* readyArray, int arraySize);
    uint32_t getHexAddress(int address);
    uint32_t getHexAddressAD(int address);
    uint32_t getSiteHex(int site);

    // void initLibMPSSE();
    bool configSPI(int pin1, int pin2, const QString &path);
    void resetPort();

Q_SIGNALS:
    void completed();
    void logMessage(const QString& message);
    void failedSignal(bool argFailed);
    // void manuFinish(bool is_finished);

protected:
    void run() override;
private:
    static FT_STATUS read_byte(uint8_t slaveAddress, uint8_t address, uint16_t *data);
    static FT_STATUS write_byte(uint8_t slaveAddress, uint8_t address, uint16_t data);
    uint8_t w25qxx_read_id(FT_HANDLE handle);
    uint8_t w25qxx_erase_byte(FT_HANDLE handle);
    uint8_t w25qxx_write_byte(FT_HANDLE handle, const QString& filePath);
    uint8_t w25qxx_read_byte(FT_HANDLE handle);
    bool w25qxx_verify_byte(FT_HANDLE handle);
    FT_DEVICE_LIST_INFO_NODE devList;
    uint8_t address = 0;
    QByteArray m_Bindata = 0;
    QDir m_dirSite;
    uint32_t m_Arr[MAX_SITE_SIZE_FLASH];
    QString m_uart1Name;
    QString m_uart2Name;
    QSerialPort *m_uart1;
    QSerialPort *m_uart2;


};


#endif
