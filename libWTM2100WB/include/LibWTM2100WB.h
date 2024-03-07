#ifndef LIBWTM2100WB_H
#define LIBWTM2100WB_H

#include <QString>

namespace LibWTM2100WB
{
QString logFileName();
QByteArray toLogData(const QString &str);
void print(const char *format, ...) Q_ATTRIBUTE_FORMAT_PRINTF(1, 2);
};

#endif // LIBWTM2100WB_H
