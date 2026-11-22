#include "barchart.h"
#include "theme.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QtMath>

BarChart::BarChart(QWidget *parent)
    : QWidget(parent), m_color(Theme::Accent)
{
    setMouseTracking(true);
    setMinimumHeight(220);
}

void BarChart::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void BarChart::setBars(const QVector<Bar> &bars)
{
    m_bars = bars;
    m_hover = -1;
    update();
}

void BarChart::setValueSuffix(const QString &suffix)
{
    m_suffix = suffix;
}

void BarChart::setColor(const QColor &color)
{
    m_color = color;
    update();
}

QSize BarChart::minimumSizeHint() const
{
    return QSize(300, 220);
}

QRectF BarChart::plotRect() const
{
    const int top = m_title.isEmpty() ? 14 : 40;
    return QRectF(64, top, width() - 64 - 16, height() - top - 44);
}

double BarChart::niceMax() const
{
    double maxV = 0;
    for (const Bar &b : m_bars)
        maxV = qMax(maxV, b.value);
    if (maxV <= 0)
        return 1;
    // округлюємо максимум до "гарного" числа для підписів осі
    const double magnitude = qPow(10, qFloor(std::log10(maxV)));
    for (double step : {1.0, 2.0, 2.5, 5.0, 10.0}) {
        if (step * magnitude >= maxV)
            return step * magnitude;
    }
    return 10 * magnitude;
}

QRectF BarChart::barRect(int i, double maxValue) const
{
    const QRectF plot = plotRect();
    const double slot = plot.width() / m_bars.size();
    const double w = qMin(slot * 0.62, 56.0);
    const double h = plot.height() * (m_bars[i].value / maxValue);
    return QRectF(plot.left() + slot * i + (slot - w) / 2, plot.bottom() - h, w, h);
}

QString BarChart::formatValue(double v) const
{
    QLocale ua(QLocale::Ukrainian, QLocale::Ukraine);
    const QString num = v >= 1000 ? ua.toString(qRound64(v)) : ua.toString(v, 'f', v == qFloor(v) ? 0 : 1);
    return m_suffix.isEmpty() ? num : num + ' ' + m_suffix;
}

void BarChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (!m_title.isEmpty()) {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 1.05);
        p.setFont(f);
        p.setPen(QColor("#1f2433"));
        p.drawText(QRectF(16, 8, width() - 32, 24), Qt::AlignLeft | Qt::AlignVCenter, m_title);
        p.setFont(font());
    }

    const QRectF plot = plotRect();
    if (m_bars.isEmpty()) {
        p.setPen(Theme::TextMuted);
        p.drawText(plot, Qt::AlignCenter, QStringLiteral("Немає даних за обраний період"));
        return;
    }

    const double maxV = niceMax();
    QFont small = font();
    small.setPointSizeF(small.pointSizeF() * 0.85);
    p.setFont(small);

    // горизонтальні лінії сітки та підписи осі
    const int lines = 4;
    for (int i = 0; i <= lines; ++i) {
        const double y = plot.bottom() - plot.height() * i / lines;
        p.setPen(QPen(QColor("#e6e9ef"), 1, i == 0 ? Qt::SolidLine : Qt::DashLine));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(Theme::TextMuted);
        p.drawText(QRectF(0, y - 10, plot.left() - 8, 20), Qt::AlignRight | Qt::AlignVCenter,
                   formatValue(maxV * i / lines).remove(' ' + m_suffix));
    }

    const double slot = plot.width() / m_bars.size();
    // якщо стовпців багато, підписуємо через один
    const int labelStep = slot < 34 ? 2 : 1;
    QFontMetrics fm(small);

    for (int i = 0; i < m_bars.size(); ++i) {
        QRectF r = barRect(i, maxV);
        QColor c = m_bars[i].color.isValid() ? m_bars[i].color : m_color;
        if (m_hover >= 0 && m_hover != i)
            c.setAlpha(110);

        if (r.height() > 0.5) {
            QPainterPath path;
            const double radius = qMin(5.0, r.height() / 2);
            path.addRoundedRect(r.adjusted(0, 0, 0, radius), radius, radius);
            p.save();
            p.setClipRect(QRectF(r.left(), r.top(), r.width(), r.height()));
            p.fillPath(path, c);
            p.restore();
        }

        if (i % labelStep == 0) {
            p.setPen(QColor("#5b6478"));
            QString label = fm.elidedText(m_bars[i].label, Qt::ElideRight, int(slot * labelStep) - 2);
            p.drawText(QRectF(plot.left() + slot * i - slot / 2, plot.bottom() + 6, slot * 2, 18),
                       Qt::AlignHCenter | Qt::AlignTop, label);
        }
    }
}

void BarChart::mouseMoveEvent(QMouseEvent *event)
{
    if (m_bars.isEmpty())
        return;
    const QRectF plot = plotRect();
    const double slot = plot.width() / m_bars.size();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
    const QPoint globalPos = event->globalPosition().toPoint();
#else
    const QPointF pos = event->localPos();
    const QPoint globalPos = event->globalPos();
#endif
    int idx = -1;
    if (pos.x() >= plot.left() && pos.x() <= plot.right() && pos.y() >= plot.top() - 10
        && pos.y() <= plot.bottom())
        idx = qBound(0, int((pos.x() - plot.left()) / slot), m_bars.size() - 1);

    if (idx != m_hover) {
        m_hover = idx;
        update();
    }
    if (idx >= 0)
        QToolTip::showText(globalPos, m_bars[idx].label + ": " + formatValue(m_bars[idx].value), this);
    else
        QToolTip::hideText();
}

void BarChart::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}
