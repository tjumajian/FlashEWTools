#include "../include/UartThread.h"
#include "../include/Settings.h"
#include "../include/BinJudge.h"
#include "../include/LibWTM2100WB.h"

#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibrary>

static QLibrary *s_libraryTunnel = nullptr;
//static uint32_t s_ReadyArray[MAX_SITE_SIZE] = {0, 0, 0, 0};
//static uint32_t s_BinArray[MAX_SITE_SIZE] = {0, 0, 0, 0};

typedef void (*TunnelFunc)(uint32_t* ReadyArray);
void (*set_ready_arr)(uint32_t* ReadyArray);
void (*get_ready_arr)(uint32_t* ReadyArray);
void (*set_bin_arr)(uint32_t* BinArray);
void (*get_bin_arr)(uint32_t* BinArray);

static QByteArray genArrJson(const char* name, const QJsonArray arr)
{
    QJsonDocument doc;
    doc.setArray(arr);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    QByteArray out;
    out.append("{\n\"");
    out.append(name);
    out.append("\":");
    out.append(json);
    out.append("\n}");
    return out;
}

template <typename T> QByteArray genArrJson(const char* name, T* array, int size=64)
{
    QJsonArray arr;
    int i = 0;
    for ( ; i <size; i++)
    {
        arr.append(QJsonValue(int(array[i])));
    }
    return genArrJson(name, arr);
}

template <typename T> QByteArray genArrJson(const char* name1,
                                            T* array1, int size1,
                                            const char* name2,
                                            T* array2, int size2)
{
    QByteArray out;
    out.append("{\n");
    {
        QJsonArray arr;
        int i = 0;
        for ( ; i <size1; i++)
        {
            arr.append(QJsonValue(int(array1[i])));
        }
        QJsonDocument doc;
        doc.setArray(arr);
        QByteArray json = doc.toJson(QJsonDocument::Compact);
        out.append("\"");
        out.append(name1);
        out.append("\":");
        out.append(json);
        out.append(",\n");
    }
    {
        QJsonArray arr;
        int i = 0;
        for ( ; i <size2; i++)
        {
            arr.append(QJsonValue(int(array2[i])));
        }
        QJsonDocument doc;
        doc.setArray(arr);
        QByteArray json = doc.toJson(QJsonDocument::Compact);
        out.append("\"");
        out.append(name2);
        out.append("\":");
        out.append(json);
        out.append("\n");
    }
    out.append("}");
    return out;
}

UartThread::UartThread(const QString &stage, QObject *parent)
    : QThread{parent},
      m_commonBins(BinJudge::commonBins(stage)),
      m_stage(stage)
{
//    int i = 0;
//    for (; i < MAX_SITE_SIZE; i++)
//    {
//        s_ReadyArray[i] = 1;
//    }
    if (s_libraryTunnel == nullptr)
    {
        QString tunnelDllPath = Settings::staticInstance()->value("tunnelDllPath").toString();
        bool ok = false;
        s_libraryTunnel = new QLibrary();
        if (!tunnelDllPath.isEmpty())
        {
            s_libraryTunnel->setFileName(tunnelDllPath);
            QByteArray logFileName = s_libraryTunnel->fileName().toUtf8();
            LibWTM2100WB::print("UartThread::UartThread: load %s", logFileName.constData());
            ok = s_libraryTunnel->load();
            if (!ok)
            {
                QByteArray logerr = s_libraryTunnel->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
            }
        }
        if (!s_libraryTunnel->isLoaded())
        {
            s_libraryTunnel->setFileName("libTunnel_32.dll");
            QByteArray logFileName = s_libraryTunnel->fileName().toUtf8();
            LibWTM2100WB::print("UartThread::UartThread: load %s", logFileName.constData());
            ok = s_libraryTunnel->load();
            if (!ok)
            {
                QByteArray logerr = s_libraryTunnel->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
            }
        }
        if (!s_libraryTunnel->isLoaded())
        {
            QDir dir = QCoreApplication::applicationDirPath();
            s_libraryTunnel->setFileName(dir.absoluteFilePath("libTunnel_32.dll"));
            QByteArray logFileName = s_libraryTunnel->fileName().toUtf8();
            LibWTM2100WB::print("UartThread::UartThread: load %s", logFileName.constData());
            ok = s_libraryTunnel->load();
            if (!ok)
            {
                QByteArray logerr = s_libraryTunnel->errorString().toUtf8();
                LibWTM2100WB::print("error: load %s failed %s",
                                    logFileName.constData(), logerr.constData());
                return;
            }
        }
        set_ready_arr = reinterpret_cast<TunnelFunc>(s_libraryTunnel->resolve("set_ready_arr"));
        get_ready_arr = reinterpret_cast<TunnelFunc>(s_libraryTunnel->resolve("get_ready_arr"));
        set_bin_arr = reinterpret_cast<TunnelFunc>(s_libraryTunnel->resolve("set_bin_arr"));
        get_bin_arr = reinterpret_cast<TunnelFunc>(s_libraryTunnel->resolve("get_bin_arr"));
    }
    memset(m_statusArray, 0, sizeof(m_statusArray));
    memset(m_initArray, 0, sizeof(m_initArray));
    memset(m_readyArray, 0, sizeof(m_readyArray));
    memset(m_testArray, 0, sizeof(m_testArray));
    get_ready_arr(m_readyArray);
//    s_ReadyArray[0] = 1;
}

void UartThread::setReadyArr(const uint32_t *ReadyArray)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::setReadyArr()");
    memcpy(m_readyArray, ReadyArray, sizeof(m_readyArray));
//    QByteArray d(reinterpret_cast<const char*>(m_readyArray), sizeof(m_readyArray));
//    qDebug() << d.toHex();
    set_ready_arr(m_readyArray);
}

void UartThread::start(const QString &uart1, const QString &uart2)
{
    m_uart1Name = uart1;
    m_uart2Name = uart2;
    QThread::start();
}

bool UartThread::openUart(const QString &uart1, const QString &uart2)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::openUart()");
    m_uart1 = new QSerialPort;
    m_uart2 = new QSerialPort;

    bool ok = true;

    m_uart1->setPortName(uart1);
    m_uart1->setBaudRate(UART_BAUDRATE);
    m_uart1->setDataBits(QSerialPort::Data8);
    m_uart1->setParity(QSerialPort::NoParity);
    m_uart1->setStopBits(QSerialPort::OneStop);
    m_uart1->setFlowControl(QSerialPort::NoFlowControl);
    ok = m_uart1->open(QIODevice::ReadWrite);
    if (!ok)
    {
        QByteArray logPort1Name = m_uart1->portName().toUtf8();
        QByteArray logerr = m_uart1->errorString().toUtf8();
        LibWTM2100WB::print("UartThread::openUart: open %s failed. %s",
                            logPort1Name.constData(), logerr.constData());
        delete m_uart1;
        delete m_uart2;
        return false;
    }

    m_uart2->setPortName(uart2);
    m_uart2->setBaudRate(UART_BAUDRATE);
    m_uart2->setDataBits(QSerialPort::Data8);
    m_uart2->setParity(QSerialPort::NoParity);
    m_uart2->setStopBits(QSerialPort::OneStop);
    m_uart2->setFlowControl(QSerialPort::NoFlowControl);
    ok = m_uart2->open(QIODevice::ReadWrite);
    if (!ok)
    {
        QByteArray logPort2Name = m_uart2->portName().toUtf8();
        QByteArray logerr = m_uart2->errorString().toUtf8();
        LibWTM2100WB::print("UartThread::openUart: open %s failed. %s",
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
    // QThread::msleep(1000);
    // QByteArray logerr1 = m_uart1->errorString().toUtf8();
    // QByteArray logerr2 = m_uart2->errorString().toUtf8();
    // LibWTM2100WB::print("UartThread::openUart: %s %s",
    //                     logerr1.constData(), logerr2.constData());
    return true;
}

bool UartThread::initFPGA()
{
    // qDebug()<<"initFPGA()";
    // //初始检查
    // m_uart2->write("INIT", 4);
    // m_uart2->flush();
    // QByteArray data;
    // while (data.size() < MAX_SITE_SIZE)
    // {
    //     bool ok = m_uart2->waitForReadyRead(5000);
    //     if (!ok)
    //     {
    //         std::string errstr = m_uart2->errorString().toStdString();
    //         LibWTM2100WB::print("UartThread::initFPGA: exec INIT failed: %s", errstr.c_str());
    //         return false;
    //     }
    //     data.append(m_uart2->readAll());
    // }
    // uint32_t readyArray[MAX_SITE_SIZE];
    // int i = 0;
    // for (i = 0; i < MAX_SITE_SIZE; i++)
    // {
    //     if (data.at(i) != INIT_CHECK_OK_CODE)   //异常的site不测试
    //     {
    //         readyArray[i] = 0;
    //     }
    //     else
    //     {
    //         readyArray[i] = m_readyArray[i];
    //     }
    // }

    // {
    //     QByteArray initDataLog = data.toHex(' ');
    //     LibWTM2100WB::print("UartThread::initFPGA: result: %s", initDataLog.constData());
    // }

    // QStringList modes = Settings::staticInstance()->value("init", "uart1").toStringList();
    // for (const QString &modeStr : modes)
    // {
    //     uchar mode = modeStr.toUShort(nullptr, 16);
    //     bool ok = initSite(m_initArray, readyArray, mode);
    //     if (!ok)
    //     {
    //         QByteArray logMode = modeStr.toUtf8();
    //         LibWTM2100WB::print("UartThread::initFPGA: error: init sites to %s",
    //                             logMode.constData());
    //         return false;
    //     }
    //     QThread::msleep(100);
    // }

    // for (i = 0; i < MAX_SITE_SIZE; i++)
    // {
    //     if ((data.at(i) != INIT_CHECK_OK_CODE) && (m_readyArray[i] > 0))
    //     {
    //         m_testArray[i] = SiteState(m_commonBins.harderror);
    //         m_statusArray[i] = STATUS_HARDERROR;
    //         LibWTM2100WB::print("UartThread::initFPGA: couldn't not init site %d.", i);
    //     }
    //     LibWTM2100WB::print("UartThread::initFPGA: %d: init check code=%d, ready=%d",
    //                         i, int(data.at(i)), int(m_readyArray[i]));
    // }
    // return true;
}

bool UartThread::test_control_init()
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::test_control_init()");

    uint32_t readyArray[MAX_SITE_SIZE];
    int i = 0;

    for (i = 0; i < MAX_SITE_SIZE; i++)
    {
        readyArray[i] = m_readyArray[i];
    }

    QStringList modes = Settings::staticInstance()->value("init", "uart1").toStringList();
    for (const QString &modeStr : modes)
    {
        uchar mode = modeStr.toUShort(nullptr, 16);
        bool ok = initSite(m_initArray, readyArray, mode);
        if (!ok)
        {
            QByteArray logMode = modeStr.toUtf8();
            LibWTM2100WB::print("UartThread::test_control_init: error: init sites to %s",
                                logMode.constData());
            return false;
        }
        QThread::msleep(100);
    }

    LibWTM2100WB::print("UartThread::test_control_init: 'ready=1' means testing the site, or ignoring");
    for (i = 0; i < MAX_SITE_SIZE; i++)
    {
        LibWTM2100WB::print("UartThread::test_control_init: %d: ready=%d", i, int(m_readyArray[i]));
    }
    return true;
}


bool UartThread::testFPGA()
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::testFPGA()");
    m_dataSizeJudged = false;
    m_t0 = QDateTime::currentMSecsSinceEpoch();
    m_dirSite = BinJudge::logDir();
    m_dirSite.cd("site_log");
    send(Settings::staticInstance()->value("init", "uart2").toString().toUtf8());
    // while (!allCompleted(m_dataRecv))
    // {
    //     QThread::msleep(100);
    //     recv(m_dataRecv);
    //     checkTimeout(m_dataRecv);
    // }
    QThread::msleep(100);
    recv(m_dataRecv);
    checkTimeout(m_dataRecv);
    return true;
}

bool UartThread::closeUart()
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::closeUart()");
    // bool ok = initSite(m_initArray, m_readyArray, 0);
    // if (!ok)
    // {
    //     LibWTM2100WB::print("UartThread::initFPGA: error: init sites to 0");
    // }
    m_uart1->setRequestToSend(false);
    m_uart2->setRequestToSend(false);
    QThread::msleep(1000);
    // QByteArray logerr1 = m_uart1->errorString().toUtf8();
    // QByteArray logerr2 = m_uart2->errorString().toUtf8();
    // LibWTM2100WB::print("UartThread::closeUart: %s %s",
    //                     logerr1.constData(), logerr2.constData());
    m_uart1->close();
    m_uart2->close();
    delete m_uart1;
    delete m_uart2;
    m_uart1 = nullptr;
    m_uart2 = nullptr;
    return true;
}

void UartThread::judgeBin()
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::judgeBin()");
    int i = 0;
    uint32_t binarr[MAX_SITE_SIZE] = {0};
    for (i=0; i < MAX_SITE_SIZE; i++)
    {
        BinJudge judge(m_stage);
        if ((m_testArray[i] == Testing) || (m_testArray[i] == TestCompleted))
        {
            int bin = judge.judge(m_dataRecv[i], i);
            binarr[i] = bin;
            m_testArray[i] = SiteState(bin);
            LibWTM2100WB::print("UartThread::judgeBin: %d : bin=%d", i, bin);
        }
        if (m_testArray[i] == m_commonBins.harderror)
        {
            LibWTM2100WB::print("UartThread::judgeBin: hardware error");
            judge.setBin(m_commonBins.harderror);
            binarr[i] = judge.currentBin();
        }
        else if (m_testArray[i] == m_commonBins.timeout)
        {
            LibWTM2100WB::print("UartThread::judgeBin: time out");
            judge.setBin(m_commonBins.timeout);
            binarr[i] = judge.currentBin();
        }
        else if (m_testArray[i] == m_commonBins.nullError)
        {
            LibWTM2100WB::print("UartThread::judgeBin: null error");
            judge.setBin(m_commonBins.nullError);
            binarr[i] = judge.currentBin();
        }
        else if (m_testArray[i] == NoTest)
        {
            LibWTM2100WB::print("UartThread::judgeBin: %d : NoTest", i);
        }
        else if (m_testArray[i] == TestCompleted)
        {
            LibWTM2100WB::print("UartThread::judgeBin: %d : Test completed", i);
        }
        else
        {
            LibWTM2100WB::print("UartThread::judgeBin: %d : %d", i, m_testArray[i]);
        }
    }
    set_bin_arr(binarr);
}

void UartThread::setSiteDir(const QDir &dir)
{
    m_dirSite = dir;
}

QDir UartThread::getDirsite()
{
    return m_dirSite;
}

void UartThread::run()
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::run()");
    // uint32_t readyArray[MAX_SITE_SIZE] = {1,1,1,1,0,0,0,0,
    //                                         1,1,1,1,0,0,0,0,
    //                                         1,1,1,1,0,0,0,0,
    //                                         0,0,0,0,0,0,0,0,
    //                                         0,0,0,0,0,0,0,0,
    //                                         0,0,0,0,0,0,0,0,
    //                                         0,0,0,0,0,0,0,0,
    //                                         0,0,0,0,0,0,0,0};
    // setReadyArr(readyArray);

    bool ok = openUart(m_uart1Name, m_uart2Name);
    if (!ok)
    {
        Q_EMIT completed();
        return;
    }
    ok = test_control_init();
    if (!ok)
    {
        Q_EMIT completed();
        return;
    }
    // ok = initFPGA();
    // if (!ok)
    // {
    //     Q_EMIT completed();
    //     return;
    // }
    ok = testFPGA();
    if (!ok)
    {
        Q_EMIT completed();
        return;
    }
    closeUart();
    judgeBin();
    QDir touchlogDir = m_dirSite;
    touchlogDir.cdUp();
    touchlogDir.mkdir("touch_log");
    touchlogDir.cd("touch_log");
    QString fileName("touch_%1_%2_%3.txt");
    QDateTime t = QDateTime::currentDateTime();
    fileName = fileName.arg(t.time().hour()).arg(t.time().minute()).arg(t.time().second());
    UartThread::writeTouch(m_dataRecv, touchlogDir.absoluteFilePath(fileName), m_stage, m_readyArray, m_statusArray);
    Q_EMIT completed();
    LibWTM2100WB::print("UartThread::run end");
}

bool UartThread::initSite(char initArray[MAX_SITE_SIZE],
                          uint32_t readyArray[MAX_SITE_SIZE],
                          uchar code)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::initSite()");
    int i = 0;
    for (; i < MAX_SITE_SIZE; i++)
    {
        initArray[i] = (readyArray[i] > 0) * code;
        if(i >= 32)
        {
            initArray[i] == (readyArray[i - 32] > 0) * code;
        }
        if (code > 0)   //code = 0为关闭模式
        {
            m_statusArray[i] = 0;
        }
        //LibWTM2100WB::print("initArray[i]: ", initArray[i]);
    }
    // for (i=33; i < 37; i++)
    // {
    //     initArray[i] =/* (readyArray[i] > 0) * */code;
    // }
    // initArray[22] = code;initArray[23] = code;
    // initArray[54] = code;initArray[55] = code;
    QByteArray data;
    data.resize(MAX_SITE_SIZE);
    memcpy(data.data(), initArray, MAX_SITE_SIZE);
    m_uart1->write(data);
    {
        QByteArray hex = data.toHex();
        LibWTM2100WB::print("UartThread::initSite: 8bits_control_data_init(FE && FF)\n7:4	3	    2	        1	    0 \nRSV	V33_EN	SPI_V33_EN	OSC_EN	RST\n%s %d", 
        hex.constData(), data.size());
    }
    m_uart1->flush();
    data.clear();
    while (data.size() < MAX_SITE_SIZE)
    {
        bool ok = m_uart1->waitForReadyRead(1000);
        if (!ok)
        {
            LibWTM2100WB::print("UartThread::initSite: wait data time out");
            return false;
        }
        data.append(m_uart1->readAll());
    }
//    if (data.size() < MAX_SITE_SIZE)
//    {
//        qDebug() << "UartThread::initSite: data.size() < MAX_SITE_SIZE" << data.size();
//        return false;
//    }
    {
        QByteArray hex = data.toHex();
        // QByteArray logErr = m_uart1->errorString().toUtf8();
        LibWTM2100WB::print("UartThread::initSite: receive uart1 status(if all 0, success )\n%x %s",
                            code, hex.constData()/*, logErr.constData()*/);
    }

    if (code > 0)   //code = 0为关闭模式
    {
        for (i=0; i < MAX_SITE_SIZE; i++)
        {
            char siteRet = data.at(i);
            if (siteRet > 0)
            {
                LibWTM2100WB::print("UartThread::initSite error: init site %d failed. code %d",
                                    i, int(siteRet));
                m_testArray[i] = SiteState(m_commonBins.harderror);
                m_statusArray[i] = siteRet;
            }
            else if (readyArray[i] == 0)
            {
                m_testArray[i] = NoTest;
            }
            else //if (m_testArray[i] != HardError)
            {
                m_testArray[i] = Testing;
            }
        }
    }
    return true;
}

bool UartThread::allCompleted(const QByteArray *dataArr)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::allCompleted()");
//    if (m_stage == "test")
//    {
//        int64_t t = QDateTime::currentMSecsSinceEpoch();
//        int64_t dt = t-m_t0;
//        if (dt > 120*1000)
//        {
//            return true;
//        }
//        else
//        {
//            return false;
//        }
//    }
    bool completed = true;    //这里需要遍历全部site
    int i = 0;
    for (; i < MAX_SITE_SIZE; i++)
    {
        if (m_testArray[i] == Testing)
        {
            if (dataArr[i].contains("FFFF"))
            {
                m_testArray[i] = TestCompleted;
            }
            else
            {
                completed = false;
            }
        }
    }
    return completed;
}

void UartThread::checkTimeout(QByteArray *dataArr)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::checkTimeout()");
    int64_t t = QDateTime::currentMSecsSinceEpoch();
    int64_t dt = t-m_t0;
//    qDebug() << dt;
    if (dt > Settings::staticInstance()->value(m_stage, "timeout").toInt()*1000)
    {
        //m_uart1->close();
        //m_uart2->close();
        int i=0;
        for (; i < MAX_SITE_SIZE; i++)
        {
            if (m_testArray[i] == Testing)
            {
                m_testArray[i] = SiteState(m_commonBins.timeout);
                // m_statusArray[i] = STATUS_TIMEOUT;
            }
        }
    }
    else if ((dt > Settings::staticInstance()->value("wait_cmp_time").toInt())
             && (!m_dataSizeJudged))
    {
        int cmp_val_min = Settings::staticInstance()->value("cmp_val_min").toInt();
        int cmp_val_max = Settings::staticInstance()->value("cmp_val_max").toInt();
        int i=0;
        for (; i < MAX_SITE_SIZE; i++)
        {
            if ((m_testArray[i] == Testing)
                    && ((dataArr[i].size() < cmp_val_min)
                        || (dataArr[i].size() > cmp_val_max)))
            {
                m_testArray[i] = SiteState(m_commonBins.timeout);
                // m_statusArray[i] = STATUS_TIMEOUT;
            }
        }
        m_dataSizeJudged = true;
    }
    if (m_stage == "test")
    {
//        if (dt > 120*1000)
        {
//            qDebug() << "time out";
            int i=0;
            for (; i < MAX_SITE_SIZE; i++)
            {
                if (m_testArray[i] == Testing)
                {
                    m_testArray[i] = SiteState(m_commonBins.timeout);
                    m_statusArray[i] = STATUS_TIMEOUT;
                }
            }
        }
    }
}

void UartThread::recv(QByteArray *dataArr)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::recv()");
    LibWTM2100WB::print("UartThread::send: send data 'RECV'");
//    m_uart2->waitForBytesWritten(1000);

    QDateTime startTime = QDateTime::currentDateTime();
    int tiknum = 0;
    while(1)
    {
        m_uart2->clearError();
        m_uart2->write("RECV", 4);
        m_uart2->flush();
        QByteArray data;
        while (data.size() < MAX_SITE_SIZE)
        {
            bool ok = m_uart2->waitForReadyRead(2000);
            if (!ok)
            {
                QByteArray logErr = m_uart2->errorString().toUtf8();
                LibWTM2100WB::print("UartThread::recv: error wait for site data failed %s",
                                    logErr.constData());
                return;
            }
            data.append(m_uart2->readAll());
        }

        int i = 0;
        int dataSize[MAX_SITE_SIZE] = {0};
        int totalSize = 0;
        for (; i < MAX_SITE_SIZE; i++)
        {
            uchar size = data.at(i);
            dataSize[i] = size;
            totalSize += dataSize[i];
        }
    //    qDebug() << "UartThread::recv: received" << data.toHex()
    //             << "totalSize=" << totalSize;
        data.remove(0, MAX_SITE_SIZE);
        while (data.size() < totalSize)
        {
            bool ok = m_uart2->waitForReadyRead(5000);
            if (!ok)
            {
                QByteArray logErr = m_uart2->errorString().toUtf8();
                LibWTM2100WB::print("UartThread::recv: error wait for site data failed %s",
                                    logErr.constData());
    //            qDebug() << "UartThread::recv: error wait for site data failed"
    //                     << m_uart2->errorString();
                return;
            }
            data.append(m_uart2->readAll());
        }
        // m_uart2->flush();
        m_uart2->waitForReadyRead(1000);
    //    qDebug() << "UartThread::recv: received" << data;
        if (tiknum == 0)
        {
            LibWTM2100WB::print("UartThread::recv: uart2 receive data from FPGA(because we sent 'RECV')");
        }
        else
        {
            LibWTM2100WB::print("UartThread::recv: uart2 receive data from FPGA the %dth time", tiknum);
        }
        for (i=0; i < MAX_SITE_SIZE; i++)
        {
            if (dataSize[i] > 0)
            {
                dataArr[i].append(data.left(dataSize[i]));
                QFile f(m_dirSite.absoluteFilePath(QString("log_%1_txt.txt").arg(i)));
                bool ok = f.open(QIODevice::Append);
                if (ok)
                {
                    f.write(data.left(dataSize[i]));
                }
                else
                {
                    QByteArray logFilename = f.fileName().toUtf8();
                    LibWTM2100WB::print("UartThread::recv: Error: Couldn't open %s for append",
                                        logFilename.constData());
                }
                data.remove(0, dataSize[i]);
                QDateTime time = QDateTime::currentDateTime();
                QByteArray logTime = time.toString("hh:mm:ss").toUtf8();
                LibWTM2100WB::print("%s: UartThread::recv: site: %d %s",
                                    logTime.constData(), i, dataArr[i].constData());
            }
        }
        QThread::msleep(1000);
        QDateTime endTime = QDateTime::currentDateTime();
        qint64 elapsedTime = startTime.msecsTo(endTime); //milliseconds
        if (elapsedTime > 180000)
        {
            break;
        }
        tiknum++;
    }
}

void UartThread::send(const QByteArray &data)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::send()");
    m_uart2->write("SEND", 4);
    m_uart2->flush();
    LibWTM2100WB::print("UartThread::send: send data 'SEND'");
    m_uart2->waitForBytesWritten(1000);
    // {
    //     QByteArray logErr = m_uart2->errorString().toUtf8();
    //     LibWTM2100WB::print("UartThread::send: send %s",
    //                         logErr.constData());
    // }
//    qDebug() << "UartThread::send: send" << m_uart2->errorString();
    QThread::msleep(500);

    uint8_t sizes[MAX_SITE_SIZE];
    int i = 0;
    int sends = 0;
    for (i=0; i < MAX_SITE_SIZE; i++)
    {
//        if (m_testArray[i] == Testing)
        {
            sizes[i] = data.size();
            sends++;
        }
//        else
//        {
//            sizes[i] = 0;
//        }
    }
    QByteArray dataSize(reinterpret_cast<char *>(sizes), MAX_SITE_SIZE);
    m_uart2->write(dataSize);
    m_uart2->flush();
    LibWTM2100WB::print("UartThread::send: send the datasize array\n %s",
                        dataSize.toHex().constData());
    m_uart2->waitForBytesWritten(2000);
    QThread::msleep(500);
//    qDebug() << "UartThread::send: send" << dataSize.toHex() << data << m_uart2->errorString();
    LibWTM2100WB::print("UartThread::send: uart2 begin to send data!");
    for (i = 0; i < MAX_SITE_SIZE; i++)
    {
        //if (m_testArray[i] == Testing)
        {
            m_uart2->write(data);
            LibWTM2100WB::print("UartThread::send: send %s",
                                data.constData());
        }
    }
    m_uart2->flush();
    m_uart2->waitForBytesWritten(3000);
    m_uart2->waitForReadyRead(2000);
    QByteArray sendRet = m_uart2->readAll();
    if (m_uart2->error() != QSerialPort::NoError)
    {
        QByteArray logErr = m_uart2->errorString().toUtf8();
        LibWTM2100WB::print("UartThread::send: send %s",
                            logErr.constData());
        QByteArray hex = data.toHex();
        LibWTM2100WB::print("UartThread::send: send %s %s",
                            hex.constData(), data.constData());
//        qDebug() << "UartThread::send: send" << dataSize.toHex() << data;
    }

    LibWTM2100WB::print("UartThread::send: uart2 receive data from FPGA(because we sent 'SEND')");
    LibWTM2100WB::print("UartThread::send: received %s",
                        sendRet.constData());
    qDebug() << sendRet.constData();
    QThread::msleep(500);
    m_uart2->clearError();
}

bool UartThread::writeTouch()
{
    QDir touchlogDir = QCoreApplication::applicationDirPath();
    touchlogDir.mkdir("touch_log");
    touchlogDir.cd("touch_log");
    QString fileName("touch_%1_%2_%3.txt");
    QDateTime t = QDateTime::currentDateTime();
    fileName = fileName.arg(t.time().hour()).arg(t.time().minute()).arg(t.time().second());
    return UartThread::writeTouch(m_dataRecv, touchlogDir.absoluteFilePath(fileName), m_stage, m_readyArray, m_statusArray);
}

bool UartThread::writeTouch(const QByteArray *siteDataArr,
                            const QString &fileName,
                            const QString &stage,
                            const uint32_t *readyArray,
                            const int *statusArray)
{
    LibWTM2100WB::print("\n");
    LibWTM2100WB::print("###########################################################################");
    LibWTM2100WB::print("Func::writeTouch()");
    const CommonBins commonBins = BinJudge::commonBins(stage);
    QVariantList readyArrayList;
    QVariantList binArrayList;
    QVariantList statusArrayList;
    QVariantList resultArrayList;
    int resultArray[MAX_SITE_SIZE] = {0};
    int binArray[MAX_SITE_SIZE] = {0};
    int i = 0;
    QByteArray jsonstr;
    for (i=0; i<MAX_SITE_SIZE; i++)
    {
        if (statusArray[i] != 0)
        {
            if (statusArray[i] == STATUS_TIMEOUT)
            {
                binArray[i] = commonBins.timeout;
            }
            else if (statusArray[i] == STATUS_HARDERROR)
            {
                binArray[i] = commonBins.harderror;
            }
            else
            {
                binArray[i] = commonBins.harderror;
            }
            resultArray[i] = 0;
            readyArrayList.append(readyArray[i]);
            binArrayList.append(binArray[i]);
            statusArrayList.append(statusArray[i]);
            resultArrayList.append(resultArray[i]);
            continue;
        }
        if (readyArray[i] == 0)
        {
            binArray[i] = 0;
            resultArray[i] = 0;
            readyArrayList.append(readyArray[i]);
            binArrayList.append(binArray[i]);
            statusArrayList.append(statusArray[i]);
            resultArrayList.append(resultArray[i]);
            continue;
        }
        BinJudge judge(stage);
        int bin = judge.judge(siteDataArr[i], i);
        QList<QByteArray> jsons = judge.outJsons();
//        qDebug() << jsons << judge.casenames();
        for (const QByteArray &json : jsons)
        {
            jsonstr.append(json);
        }
        binArray[i] = bin;
        // resultArray[i] = jsons.size() > 0;
        resultArray[i] = (binArray[i] == 1) ? 1 : 0;
        readyArrayList.append(readyArray[i]);
        binArrayList.append(binArray[i]);
        statusArrayList.append(statusArray[i]);
        resultArrayList.append(resultArray[i]);
        LibWTM2100WB::print("UartThread::writeTouch: %d, bin=%d, status=%d, ready=%d, result=%d",
                            i, bin, statusArray[i], int(readyArray[i]), resultArray[i]);
//        qDebug() << "UartThread::writeTouch" << i << ", bin=" << bin;
    }
    QFile fTouch(fileName);
    bool ok = fTouch.open(QIODevice::WriteOnly);
    if (!ok)
    {
        QByteArray logFilename = fileName.toUtf8();
        LibWTM2100WB::print("UartThread::writeTouch error: couldn't open file %s for write",
                            logFilename.constData());
//        qDebug() << "UartThread::writeTouch error: couldn't open file" << fileName
//                 << "for write";
        return false;
    }
    QByteArray out = genArrJson<const uint32_t>("ready",readyArray);
    fTouch.write(out);
    fTouch.write("\n", 1);
    out = genArrJson<const int>("status",statusArray);
    fTouch.write(out);
    fTouch.write("\n", 1);
    QStringList cases = Settings::staticInstance()->value(stage, "casename").toStringList();
    out = genArrJson("casename",QJsonArray::fromStringList(cases));
    fTouch.write(out);
    fTouch.write("\n", 1);
    fTouch.write(jsonstr);
    out = genArrJson<int>("bin",binArray,64,
                          "result",resultArray,64);
    fTouch.write(out);
    fTouch.write("\n", 1);
    return true;
}
