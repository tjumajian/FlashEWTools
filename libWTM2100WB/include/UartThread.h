#ifndef UARTTHREAD_H
#define UARTTHREAD_H

#include "BinJudge.h"
#include <QDir>
#include <QThread>
#include <QSerialPort>
#include <iostream>

enum
{
    MAX_SITE_SIZE = 64,
    STATUS_TIMEOUT = 2000,
    STATUS_HARDERROR = 2001,
    INIT_CHECK_OK_CODE = 0x70,
    UART_BAUDRATE = 115200,
//    INIT_STATE_SITE = 0xd,
};

class UartThread : public QThread
{
    Q_OBJECT
public:
    enum SiteState
    {
        Timeout = -2,
        Testing = -1,
        NoTest = 0,
        TestCompleted = 1,
    };
    explicit UartThread(const QString &stage, QObject *parent = nullptr);

    void setReadyArr(const uint32_t* ReadyArray);
    QDir getDirsite();
    void start(const QString &uart1, const QString &uart2);

    bool openUart(const QString &uart1, const QString &uart2);
    bool initFPGA();
    bool test_control_init();
    bool testFPGA();
    bool closeUart();
    void judgeBin();
    void setSiteDir(const QDir &dir);

    bool writeTouch();
    static bool writeTouch(const QByteArray *siteDataArr,  //QByteArray [MAX_SITE_SIZE]
                           const QString &fileName,
                           const QString &stage,
                           const uint32_t *readyArray,  //const uint32_t [MAX_SITE_SIZE]
                           const int *statusArray);  //const int [MAX_SITE_SIZE]

Q_SIGNALS:
    void completed();

protected:
    void run() override;

private:
    bool initSite(char initArray[MAX_SITE_SIZE],
                  uint32_t readyArray[MAX_SITE_SIZE],
                  uchar code);
    bool allCompleted(const QByteArray *dataArr);  //QByteArray [MAX_SITE_SIZE]
    void checkTimeout(QByteArray *dataArr);  //QByteArray [MAX_SITE_SIZE]
    void recv(QByteArray *dataArr);  //QByteArray [MAX_SITE_SIZE]
    void send(const QByteArray &data);

    const CommonBins m_commonBins;
    QByteArray m_dataRecv[MAX_SITE_SIZE];
    int m_statusArray[MAX_SITE_SIZE];
    char m_initArray[MAX_SITE_SIZE];
    uint32_t m_readyArray[MAX_SITE_SIZE];
    SiteState m_testArray[MAX_SITE_SIZE];
    QSerialPort *m_uart1;
    QSerialPort *m_uart2;

    QDir m_dirSite;
    QString m_stage;
    QString m_uart1Name;
    QString m_uart2Name;

    int64_t m_t0 = 0;  //ms
    bool m_dataSizeJudged = false;
};

#endif // UARTTHREAD_H
