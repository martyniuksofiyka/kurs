#ifndef BARCHART_H
#define BARCHART_H

#include <QColor>
#include <QVector>
#include <QWidget>

// Проста стовпчикова діаграма. Малюється вручну через QPainter,
// тому не потребує модуля Qt Charts.
class BarChart : public QWidget
{
    Q_OBJECT
public:
    struct Bar {
        QString label;
        double value = 0;
        QColor color;     // якщо не задано - колір за замовчуванням
    };

    explicit BarChart(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setBars(const QVector<Bar> &bars);
    void setValueSuffix(const QString &suffix);
    void setColor(const QColor &color);

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF plotRect() const;
    QRectF barRect(int i, double maxValue) const;
    double niceMax() const;
    QString formatValue(double v) const;

    QString m_title;
    QString m_suffix;
    QColor m_color;
    QVector<Bar> m_bars;
    int m_hover = -1;
};

#endif // BARCHART_H
