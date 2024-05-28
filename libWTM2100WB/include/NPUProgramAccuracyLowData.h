#ifndef NPUPROGRAMACCURACYLOWDATA_H
#define NPUPROGRAMACCURACYLOWDATA_H

#include <QMap>

class Settings;
class NPUProgramAccuracyLowData
{
public:
    enum FilterType
    {
        Filter_None = 0,
        Filter_Maximum = 1,
        Filter_Error = 2,
    };

    struct Filter
    {
        double value;
        int x;
        int y;
        FilterType type;
    };

    NPUProgramAccuracyLowData();

    void changeSettings(Settings *settings, const QString &stage);

    void reload();
    int rows() const;
    int columns() const;

    FilterType filterType(int x, int y) const;
    double filterValue(int x, int y) const;

    void setFilter(int x, int y, double value, FilterType type);
    bool valueOk(int index, double value) const;

private:
    QMap<QString, Filter> m_data;
    QString m_stage;
    Settings *m_settings;
    int m_rows;
    int m_columns;
};

#endif // NPUPROGRAMACCURACYLOWDATA_H
