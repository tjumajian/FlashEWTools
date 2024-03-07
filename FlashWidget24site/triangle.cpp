#include "triangle.h"

TriangleWidget::TriangleWidget(QWidget* parent) :
    QWidget(parent),
    m_triangleColor(Qt::white)
{
    setFixedSize(210, 180);
    triangfleName = "NoTest";
}

void TriangleWidget::paintEvent(QPaintEvent* event)
{
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        // 获取控件的大小
        int width = this->width();
        int height = this->height();

        // 定义三角形的坐标点
        QPolygon triangle;
        triangle << QPoint(width - 80, height / 2)
                 << QPoint(width, 0)
                 << QPoint(width, height);

        // 设置三角形的填充颜色
        painter.setBrush(m_triangleColor);

        // 绘制三角形
        painter.drawPolygon(triangle);

        QFont font("Arial", 12);
        QRect textRect(width - 60, (height / 2)-10, 60, 20);
        painter.setFont(font);
        painter.drawText(textRect, triangfleName);
}

void TriangleWidget::setTriangleColor(const QColor& color)
{
    m_triangleColor = color;
    update();
}

void TriangleWidget::setTriangleName(const QString& name)
{
    triangfleName = name;
    update(); // 更新窗口内容
}

void TriangleWidget::setBackgroundColor(const QColor& color)
{
    backgroundColor = color;
    update(); // 通知部件进行重绘
}

QColor TriangleWidget::getBackgroundColor() const
{
    return backgroundColor;
}

void TriangleWidget::setTextColor(const QColor& color)
{
    textColor = color;
    update(); // 通知部件进行重绘
}

QColor TriangleWidget::getTextColor() const
{
    return textColor;
}

