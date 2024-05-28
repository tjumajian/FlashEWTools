#include "../include/LibWTM2100WB.h"
#include "../include/Settings.h"
#include <QFile>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <iostream>

static QString s_logFileName;

namespace LibWTM2100WB
{
QString logFileName()
{
    if (s_logFileName.isEmpty())
    {
        QDir dir(QCoreApplication::instance()->applicationDirPath());
        QString stage = Settings::staticInstance()->value("stage").toString();
        dir.mkpath("log");
        dir.cd("log");
        s_logFileName = dir.absoluteFilePath(
                    QString("log_%1_%2.log")
                    .arg(stage, QDateTime::currentDateTime().toString("hhmmss")));
    }
    return s_logFileName;
}

QByteArray toLogData(const QString &str)
{
    return str.toUtf8();
}

void print(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    QString log = QString::vasprintf(format, args);
    va_end(args);
//    qDebug() << log;
    QByteArray logData = log.toLocal8Bit();
    std::cout << logData.constData() << std::endl;
    QFile f(logFileName());
    f.open(QIODevice::Append);
    f.write(toLogData(log));
    f.write("\r\n");
}
};
