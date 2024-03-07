#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <QWidget>
#include <QPainter>
#include <QPolygon>
#include <QDebug>

class TriangleWidget : public QWidget
{
    Q_OBJECT
public:
    TriangleWidget(QWidget* parent = nullptr);
    // 公共函数，用于设置三角形的颜色
    void setTriangleColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    QColor getBackgroundColor() const;
    void setTextColor(const QColor& color);
    QColor getTextColor() const;
    void setTriangleName(const QString& name);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_triangleColor;
    QColor backgroundColor;
    QColor textColor;
    QString triangfleName;
};

#endif