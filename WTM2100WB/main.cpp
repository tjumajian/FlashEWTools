#include <QCoreApplication>
#include "Settings.h"
#include "UartThread.h"
#include "FlashThread.h"
#include "LibWTM2100WB.h"
#include "ftd2xx.h"
#include "libMPSSE_spi.h"
#include <iostream>

int main(int argc, char *argv[])
{
    // if (argc < 3)
    // {
    //     std::cout << "Usage: " << argv[0] << " [com1] [com2]" << std::endl;
    //     return 0;
    // }
    QCoreApplication a(argc, argv);

    QString com1("COM7");
    QString com2("COM8");

    QString stage = Settings::staticInstance()->value("stage").toString();
    {
        QByteArray logStage = stage.toUtf8();
        LibWTM2100WB::print("stage=%s", logStage.constData());  
    }
    qDebug() << stage;
    if (stage == "Flash")
    {
        FlashThread *flashthread = new FlashThread;
        flashthread->start(com1, com2);
        flashthread->wait();
    }
    if  (stage == "CP2" || stage == "CP3")
    {
        // QString com1(argv[1]);
        // QString com2(argv[2]);
        UartThread *uartthread = new UartThread(stage);
        QDir sitedir = QCoreApplication::applicationDirPath();
        BinJudge::setLogDir(sitedir);
        if (sitedir.cd("site_log"))
        {
            sitedir.removeRecursively();
            sitedir.setPath(QCoreApplication::applicationDirPath());
        }
        sitedir.mkdir("site_log");
        sitedir.cd("site_log");
        uartthread->setSiteDir(sitedir);
        uartthread->start(com1, com2);
        uartthread->wait();
    }
    
    // LibWTM2100WB::print("%s: end", argv[0]);

    return 0;
}
