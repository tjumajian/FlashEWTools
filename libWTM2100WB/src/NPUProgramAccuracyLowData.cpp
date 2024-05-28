#include "../include/NPUProgramAccuracyLowData.h"
#include "../include/Settings.h"

NPUProgramAccuracyLowData::NPUProgramAccuracyLowData()
{
    m_settings = Settings::staticInstance();
    m_stage = "CP3";
    reload();
}

void NPUProgramAccuracyLowData::changeSettings(Settings *settings, const QString &stage)
{
    m_settings = settings;
    m_stage = stage;
    reload();
}

void NPUProgramAccuracyLowData::reload()
{
    m_data.clear();
    QVariantList filters = m_settings->value(m_stage, "checkMatrix").toList();
    for (const QVariant &f : filters)
    {
        QVariantMap filterMap = f.toMap();
        Filter filter;
        filter.x = filterMap.value("x").toInt();
        filter.y = filterMap.value("y").toInt();
        filter.type = static_cast<FilterType>(filterMap.value("type").toInt());
        filter.value = filterMap.value("value").toDouble();
        QString key = QString("%1,%2").arg(filter.x).arg(filter.y);
        m_data[key] = filter;
    }
    m_rows = m_settings->value(m_stage, "testDataRows").toInt();
    m_columns = m_settings->value(m_stage, "testDataColumns").toInt();
}

int NPUProgramAccuracyLowData::rows() const
{
    return m_rows;
}

int NPUProgramAccuracyLowData::columns() const
{
    return m_columns;
}

NPUProgramAccuracyLowData::FilterType NPUProgramAccuracyLowData::filterType(int x, int y) const
{
    QString key = QString("%1,%2").arg(x).arg(y);
    if (m_data.contains(key))
    {
        return m_data.value(key).type;
    }
    return Filter_None;
}

double NPUProgramAccuracyLowData::filterValue(int x, int y) const
{
    QString key = QString("%1,%2").arg(x).arg(y);
    if (m_data.contains(key))
    {
        return m_data.value(key).value;
    }
    return qQNaN();
}

void NPUProgramAccuracyLowData::setFilter(int x, int y, double value, FilterType type)
{
    QString key = QString("%1,%2").arg(x).arg(y);
    Filter f;
    if (m_data.contains(key))
    {
        f = m_data.value(key);
    }
    else
    {
        f.x = x;
        f.y = y;
    }
    f.type = type;
    f.value = value;
    if (type == Filter_None)
    {
        m_data.remove(key);
    }
    else
    {
        m_data[key] = f;
    }
    QVariantList filters = m_settings->value(m_stage, "checkMatrix").toList();
    int i = 0;
    bool found = false;
    for (; i < filters.size(); i++)
    {
        QVariantMap filter = filters.at(i).toMap();
        if ((filter.value("x").toInt() == f.x)
                && (filter.value("y").toInt() == f.y))
        {
            if (type == Filter_None)
            {
                m_data.remove(key);
            }
            else
            {
                filter["x"] = f.x;
                filter["y"] = f.y;
                filter["type"] = int(f.type);
                filter["value"] = f.value;
                filters[i] = filter;
                m_data[key] = f;
            }
            found = true;
            break;
        }
    }
    if (!found)
    {
        QVariantMap filter;
        filter["x"] = f.x;
        filter["y"] = f.y;
        filter["type"] = int(f.type);
        filter["value"] = f.value;
        filters.append(filter);
    }
    m_settings->setValue(m_stage, "checkMatrix", filters);
    //m_settings->save();
}

bool NPUProgramAccuracyLowData::valueOk(int index, double value) const
{
    int columns = m_settings->value(m_stage, "testDataColumns").toInt();
    int x = index % columns;
    int y = index / columns;
    QString key = QString("%1,%2").arg(x).arg(y);
    if (!m_data.contains(key))
    {
        return true;
    }
    double valueCompare = m_data.value(key).value;
    switch (m_data.value(key).type)
    {
    case NPUProgramAccuracyLowData::Filter_None:
        return true;
    case NPUProgramAccuracyLowData::Filter_Maximum:
        return value < valueCompare+0.000000000001;
    case NPUProgramAccuracyLowData::Filter_Error:
        return (value > 1.0-valueCompare-0.000000000001) && (value < 1.0+valueCompare+0.000000000001);
    }
    return true;
}
