#include "../include/BinJudge.h"
#include "../include/Settings.h"
#include "../include/NPUProgramAccuracyLowData.h"
#include "../include/LibWTM2100WB.h"
#include <QBuffer>
#include <QDebug>
#include <QJsonDocument>
#include <QSet>

static bool compareBinInfo(const BinInfo &a, const BinInfo &b)
{
    return a.priority < b.priority;
}

QDir BinJudge::m_logDir;

BinJudge::BinJudge(const QString &stage) :
    m_commonBins(commonBins(stage)),
    m_stage(stage)
{
    QVariantList bins = Settings::staticInstance()->value(stage, "BinMap").toList();
    auto iter = bins.cbegin();
    for (; iter != bins.cend(); ++iter)
    {
        BinInfo info;
        QVariantMap object = iter->toMap();
        info.bin = object.value("bin").toInt();
        info.priority = object.value("priority").toInt();
        info.description = object.value("description").toString();
        info.key = object.value("key").toString();
        info.type = object.value("type").toString();
        m_binMap.append(info);
    }
    std::sort(m_binMap.begin(), m_binMap.end(), compareBinInfo);  //从高优先级到低优先级排序
//    int i = 0;
//    for (; i < m_binMap.size(); i++)
//    {
//        qDebug() << m_binMap.at(i).bin << m_binMap.at(i).key << m_binMap.at(i).priority;
//    }
    m_normalKeys = Settings::staticInstance()->value(stage, "keyShouldExist").toStringList();
    m_normalKeySet = QSet<QString>(m_normalKeys.begin(), m_normalKeys.end());
}

void BinJudge::setBin(int bin)
{
    int priority = binPriority(bin);
    if (m_currentPriority >= priority)
    {
        m_bin = bin;
        m_currentPriority = priority;
    }
}

int BinJudge::judge(const QByteArray &data, int site)
{
    int i = 0;
    m_data = data;
    while (m_data.contains('\0'))
    {  //移除所有0字符
        m_data.remove(m_data.indexOf('\0'), 1);
    }
    m_data = m_data.simplified();
    if (m_data.isEmpty())
    {
        setBin(m_commonBins.nullError);
        LibWTM2100WB::print("BinJudge::judge data is empty in site:%d", site);
        return m_bin;
    }
    QString firmwareVersion = Settings::staticInstance()->value("FirmwareVersion").toString();
    if (!data.contains(firmwareVersion.toUtf8()))
    {
        setBin(m_commonBins.firmwareversion);
        LibWTM2100WB::print("BinJudge::judge FirmwareVersion check failed in site:%d", site);
        return m_bin;
    }
    // QString sendData = Settings::staticInstance()->value("init", "uart2").toString();
    // if (!data.contains(sendData.toUtf8()))
    // {
    //     setBin(m_commonBins.parse);
    //     LibWTM2100WB::print("BinJudge::judge sendData check failed in site:%d", site);
    //     return m_bin;
    // }
    QString str = QString::fromUtf8(data);
    for (; i < m_binMap.size(); i++)
    {
        if (m_binMap.at(i).priority >= m_currentPriority)
        {   //因为优先级从高优先级到低排序，所以直接跳过
            break;
        }
        if ((!m_binMap.at(i).key.isEmpty())
                && str.contains(m_binMap.at(i).key, Qt::CaseInsensitive)
                && (m_binMap.at(i).priority < m_currentPriority))
        {  //异常bin值判断
            m_bin = m_binMap.at(i).bin;
            m_currentPriority = m_binMap.at(i).priority;
            break;
        }
    }
    {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(m_data, &err);
        while ((err.error != QJsonParseError::NoError)
               && (!m_data.isEmpty()))  //json拆分
        {
            switch (err.error)
            {
            case QJsonParseError::NoError:  //如果还有json，会遇到GarbageAtEnd异常，所以noerror则为解析完成。
                m_data.clear();
                break;
            case QJsonParseError::UnterminatedString:
                LibWTM2100WB::print("BinJudge::judge JsonParseError: UnterminatedString offset:%d,"
                                    " end char code:%d. "
                                    "Remnant data will be ignored. %s",
                                    err.offset, int(m_data.at(err.offset)),
                                    m_data.constData());
                m_data.clear();
                setBin(m_commonBins.parse);

                break;
            case QJsonParseError::IllegalValue:

                m_data.remove(0, err.offset);
                doc = QJsonDocument::fromJson(m_data, &err);
                break;
            case QJsonParseError::GarbageAtEnd:
            {
                QByteArray d = m_data.mid(0, err.offset);

                err.error = QJsonParseError::NoError;
                err.offset = 0;
                doc = QJsonDocument::fromJson(d, &err);

                checkJson(doc.object(), site);
                m_data.remove(0, d.size());
                err.error = QJsonParseError::NoError;
                err.offset = 0;
                doc = QJsonDocument::fromJson(m_data, &err);

            }
                break;
            default:
                m_data.clear();
            }
        }
        checkJson(doc.object(), site);

        if (m_jsonsOut.isEmpty())
        {
            setBin(m_commonBins.nullError);
            LibWTM2100WB::print("BinJudge::judge data is empty (split json failed) in site:%d", site);
        }
    }

    if (m_bin == 1)
    {
        QString str = QString::fromUtf8(data);
        for (const QString &key : qAsConst(m_normalKeys))  //检查正常值是否齐全
        {
            if (str.contains(key.toUtf8(), Qt::CaseInsensitive))
            {
                m_normalKeySet.remove(key);

            }
        }
        if (!m_normalKeySet.empty())
        {
            setBin(m_commonBins.parse);

            {
                QString looseKeys;
                QDebug d(&looseKeys);
                d << m_normalKeySet;
                QByteArray log = looseKeys.toUtf8();
                LibWTM2100WB::print("BinJudge::judge loose key %s",
                                    log.constData());
            }

        }
    }
    if ((m_commonBins.finalcheckerror > 0) && (!data.contains("FFFF")))
    {
        LibWTM2100WB::print("BinJudge::judge warning: \"FFFF\" not found. Bin will judge to time out.");
        setBin(m_commonBins.finalcheckerror);

    }
    return m_bin;
}

QDir BinJudge::logDir()
{
    return m_logDir;
}

void BinJudge::setLogDir(const QDir &dir)
{
    m_logDir = dir;
}

CommonBins BinJudge::commonBins(const QString &stage)
{
    CommonBins ret;
    QVariantList bins = Settings::staticInstance()->value(stage, "BinMap").toList();
    auto iter = bins.cbegin();
    for (; iter != bins.cend(); ++iter)
    {
        QVariantMap object = iter->toMap();
        int bin = object.value("bin").toInt();
        QString type = object.value("type").toString();
        if (type == "harderror")
        {
            ret.harderror = bin;
        }
        else if (type == "null")
        {
            ret.nullError = bin;
        }
        else if (type == "timeout")
        {
            ret.timeout = bin;
        }
        else if (type == "parse")
        {
            ret.parse = bin;
        }
        else if (type == "firmwareversion")
        {
            ret.firmwareversion = bin;
        }
        else if (type == "finalcheckerror")
        {
            ret.finalcheckerror = bin;
        }
        else if (type == "npulow")
        {
            ret.npulow = bin;
        }
    }
    return ret;
}

int BinJudge::binPriority(int bin) const
{
    auto iter = m_binMap.cbegin();
    for (; iter != m_binMap.cend(); ++iter)
    {
        if (iter->bin == bin)
        {
            return iter->priority;
        }
    }
    return INT_MAX;
}

int BinJudge::checkJson(const QJsonObject &obj, int site)
{
    QJsonObject objOut = obj;
    if (obj.contains("test data"))
    {
        QString caseStr = obj.value("case").toString();
        QString data = obj.value("test data").toString();
        objOut["case"] = caseStr;
        objOut["site"] = QString::number(site);
        objOut["datetime"] = QDateTime::currentDateTime().toString();
        if (m_stage.compare("CP3", Qt::CaseInsensitive) == 0)
        {
            caseStr = caseStr.simplified();
            if (caseStr.contains("SATA1", Qt::CaseInsensitive)
                    || caseStr.contains("STAT1", Qt::CaseInsensitive))
            {
                //if ((Settings::staticInstance()->value("check_npu").toBool()))
                if (m_commonBins.npulow > 0)
                {
                    {
                        QByteArray log = caseStr.toUtf8();
                        LibWTM2100WB::print("site %d: checkNPUProgramAccuracyLow %s", site, log.constData());
                    }
                    //                qDebug() << site << "checkNPUProgramAccuracyLow" << caseStr;
                    checkNPUProgramAccuracyLow(data);
                }
            }
            else if ((m_commonBins.finalcheckerror > 0) && (caseStr == "FFFF"))
            {
                if (data != "0")
                {
                    //setBin(40);
                    setBin(m_commonBins.finalcheckerror);
                    QByteArray log = data.toUtf8();
                    LibWTM2100WB::print("BinJudge::judge Final Check Fail %s", log.constData());
                }
            }
            else if (!data.contains(','))
            {
            }
        }
        else
        {
            caseStr = caseStr.simplified();
            if (!data.contains(','))
            {
            }
        }
        QJsonDocument doc;
        doc.setObject(objOut);
        m_jsonsOut.append(doc.toJson(QJsonDocument::Indented));
    }
    return m_bin;
}

int BinJudge::checkNPUProgramAccuracyLow(const QString &testData)
{
    int priority39 = binPriority(m_commonBins.npulow);
    int priority41 = binPriority(m_commonBins.parse);
    if (m_currentPriority <= qMin(priority39, priority41))
    {
        return m_bin;
    }
//    if (m_currentPriority <= priority41)
//    {
//        return m_bin;
//    }
    QStringList dataStr = testData.split(',', Qt::SkipEmptyParts);
    //QVector<int> data(dataStr.size());
    int i = 0;
    NPUProgramAccuracyLowData data;
    bool ok = true;
    if (dataStr.size() < data.rows()*data.columns())
    {
        setBin(m_commonBins.parse);
        LibWTM2100WB::print("BinJudge::judge data not enough. data size=%d", dataStr.size());
        return m_bin;
    }
    for (; (i < dataStr.size()) && ok; i++)
    {
        bool valueok = data.valueOk(i, dataStr.at(i).toDouble());
        ok = ok & valueok;
        //data[i] = dataStr.at(i).toInt();
        if (!valueok)
        {
            QByteArray logData = dataStr.at(i).toUtf8();
            LibWTM2100WB::print("checkNPUProgramAccuracyLow x=%d, y=%d, data=%s %lf",
                                i/data.columns(), i%data.columns(),
                                logData.constData(), dataStr.at(i).toDouble());
//            qDebug() << "checkNPUProgramAccuracyLow:"
//                     << i / data.columns() << i % data.columns()
//                     << dataStr.at(i) << dataStr.at(i).toDouble();
        }
    }
    if (!ok)
    {
        setBin(m_commonBins.npulow);
    }
    return m_bin;
}
