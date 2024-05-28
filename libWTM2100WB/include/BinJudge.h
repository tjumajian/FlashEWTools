#ifndef BINJUDGE_H
#define BINJUDGE_H

#include <QDir>
#include <QStringList>
#include <QJsonObject>
#include <QMap>

struct BinInfo
{
    QString key;
    QString description;
    QString type;
    int bin = 0;
    int priority = 0;
};

struct CommonBins
{
    int harderror = 19;
    int nullError;
    int timeout;
    int firmwareversion;
    int parse;
    int finalcheckerror = -1;
    int npulow = -1;
};

class BinJudge
{
public:
    BinJudge(const QString &stage);

    int currentBin() const
    {
        return m_bin;
    }

    void setBin(int bin);

    int judge(const QByteArray &data, int site);
    QList<QByteArray> outJsons() const
    {
        return m_jsonsOut;
    }

    static QDir logDir();
    static void setLogDir(const QDir &dir);
    static CommonBins commonBins(const QString &stage);

private:
    int binPriority(int bin) const;
    int checkJson(const QJsonObject &obj, int site);
    int checkNPUProgramAccuracyLow(const QString &testData);

    const CommonBins m_commonBins;
    QList<BinInfo> m_binMap;
    QList<QByteArray> m_jsonsOut;
    QByteArray m_data;
    QString m_stage;
    QSet<QString> m_normalKeySet;
    QStringList m_normalKeys;
    int m_bin = 1;
    int m_currentPriority = INT_MAX;
    static QDir m_logDir;
};

#endif // BINJUDGE_H
