#include "schedulewidget.h"
#include "core/exporter.h"
#include "theme.h"
#include "widgets.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

ScheduleWidget::ScheduleWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ScheduleWidget::setData(const QDate &date, const QVector<Hall> &halls,
                             const QVector<Booking> &bookings, const QTime &open, const QTime &close)
{
    m_date = date;
    m_halls = halls;
    m_bookings = bookings;
    m_open = open;
    m_close = close;
    m_hoverBooking = -1;
    m_hoverSlot = QRectF();
    setFixedHeight(sizeHint().height());
    update();
}

QSize ScheduleWidget::sizeHint() const
{
    return QSize(TimeColumn + 160 * qMax(1, m_halls.size()), HeaderHeight + slotCount() * SlotHeight + 2);
}

int ScheduleWidget::slotCount() const
{
    return qMax(1, m_open.secsTo(m_close) / 1800);
}

double ScheduleWidget::columnWidth() const
{
    return (width() - TimeColumn) / double(qMax(1, m_halls.size()));
}

double ScheduleWidget::yForTime(const QTime &t) const
{
    return HeaderHeight + m_open.secsTo(t) / 1800.0 * SlotHeight;
}

QRectF ScheduleWidget::bookingRect(const Booking &b) const
{
    int col = -1;
    for (int i = 0; i < m_halls.size(); ++i) {
        if (m_halls[i].id == b.hallId)
            col = i;
    }
    if (col < 0)
        return QRectF();
    const QTime from = qMax(b.from.time(), m_open);
    const QTime to = b.to.date() > m_date ? m_close : qMin(b.to.time(), m_close);
    const double x = TimeColumn + col * columnWidth();
    return QRectF(x + 4, yForTime(from) + 1, columnWidth() - 8, yForTime(to) - yForTime(from) - 2);
}

int ScheduleWidget::bookingAt(const QPointF &pos) const
{
    for (int i = 0; i < m_bookings.size(); ++i) {
        if (bookingRect(m_bookings[i]).contains(pos))
            return i;
    }
    return -1;
}

bool ScheduleWidget::slotAt(const QPointF &pos, int *hallId, QDateTime *start) const
{
    if (pos.x() < TimeColumn || pos.y() < HeaderHeight || m_halls.isEmpty())
        return false;
    const int col = int((pos.x() - TimeColumn) / columnWidth());
    const int row = int((pos.y() - HeaderHeight) / SlotHeight);
    if (col < 0 || col >= m_halls.size() || row < 0 || row >= slotCount())
        return false;
    *hallId = m_halls[col].id;
    *start = QDateTime(m_date, m_open.addSecs(row * 1800));
    return true;
}

void ScheduleWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);

    const double colW = columnWidth();
    const int slotTotal = slotCount();

    // заголовки залів
    QFont bold = font();
    bold.setBold(true);
    for (int i = 0; i < m_halls.size(); ++i) {
        QRectF r(TimeColumn + i * colW, 0, colW, HeaderHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(m_halls[i].color);
        p.drawEllipse(QPointF(r.left() + 16, r.center().y()), 5, 5);
        p.setPen(QColor("#1f2433"));
        p.setFont(bold);
        p.drawText(r.adjusted(28, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(m_halls[i].name, Qt::ElideRight, int(colW) - 34));
    }
    p.setFont(font());

    // вихідні дні підсвічуємо, бо в них діє надбавка
    if (m_date.dayOfWeek() >= 6)
        p.fillRect(QRectF(TimeColumn, HeaderHeight, width() - TimeColumn, slotTotal * SlotHeight), QColor("#fbf8f1"));

    // лінії сітки та підписи часу
    for (int s = 0; s <= slotTotal; ++s) {
        const double y = HeaderHeight + s * SlotHeight;
        const bool fullHour = s % 2 == 0;
        p.setPen(QPen(fullHour ? QColor("#e3e6ec") : QColor("#f0f2f5"), 1));
        p.drawLine(QPointF(TimeColumn, y), QPointF(width(), y));
        if (fullHour && s < slotTotal) {
            p.setPen(Theme::TextMuted);
            p.drawText(QRectF(0, y - 8, TimeColumn - 10, 18), Qt::AlignRight | Qt::AlignVCenter,
                       m_open.addSecs(s * 1800).toString("HH:mm"));
        }
    }
    p.setPen(QPen(QColor("#e3e6ec"), 1));
    for (int i = 0; i <= m_halls.size(); ++i) {
        const double x = TimeColumn + i * colW;
        p.drawLine(QPointF(x, 0), QPointF(x, HeaderHeight + slotTotal * SlotHeight));
    }

    // підсвічування вільного слоту під курсором
    if (!m_hoverSlot.isNull()) {
        QColor c = Theme::Accent;
        c.setAlpha(22);
        p.fillRect(m_hoverSlot, c);
        p.setPen(Theme::Accent);
        p.drawText(m_hoverSlot, Qt::AlignCenter, QStringLiteral("+ бронювання"));
    }

    // блоки бронювань
    for (int i = 0; i < m_bookings.size(); ++i) {
        const Booking &b = m_bookings[i];
        QRectF r = bookingRect(b);
        if (r.isNull())
            continue;
        QColor base;
        for (const Hall &h : m_halls) {
            if (h.id == b.hallId)
                base = h.color;
        }

        QColor fill = base;
        fill.setAlpha(b.status == "completed" ? 60 : 38);
        if (i == m_hoverBooking)
            fill.setAlpha(fill.alpha() + 40);

        QPainterPath path;
        path.addRoundedRect(r, 6, 6);
        p.fillPath(path, fill);
        // нові (непідтверджені) бронювання позначаємо штрихуванням
        if (b.status == "new") {
            QColor hatch = base;
            hatch.setAlpha(45);
            p.fillPath(path, QBrush(hatch, Qt::BDiagPattern));
        }
        p.fillRect(QRectF(r.left(), r.top() + 3, 4, r.height() - 6), base);

        p.setPen(base.darker(160));
        QRectF text = r.adjusted(10, 4, -6, -4);
        p.setFont(bold);
        const QString time = b.from.time().toString("HH:mm") + "–" + b.to.time().toString("HH:mm");
        p.drawText(text, Qt::AlignTop | Qt::AlignLeft, time);
        p.setFont(font());
        if (r.height() > 36) {
            p.drawText(text.adjusted(0, 18, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                       p.fontMetrics().elidedText(b.client, Qt::ElideRight, int(text.width())));
        }
        if (r.height() > 58) {
            p.setPen(Theme::TextMuted);
            p.drawText(text.adjusted(0, 36, 0, 0), Qt::AlignTop | Qt::AlignLeft, Ui::bookingStatusText(b.status));
        }
    }

    // лінія поточного часу
    const QTime now = QTime::currentTime();
    if (m_date == QDate::currentDate() && now > m_open && now < m_close) {
        const double y = yForTime(now);
        p.setPen(QPen(Theme::Danger, 2));
        p.drawLine(QPointF(TimeColumn, y), QPointF(width(), y));
        p.setBrush(Theme::Danger);
        p.drawEllipse(QPointF(TimeColumn, y), 4, 4);
    }
}

void ScheduleWidget::mouseMoveEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
    const QPoint globalPos = event->globalPosition().toPoint();
#else
    const QPointF pos = event->localPos();
    const QPoint globalPos = event->globalPos();
#endif
    const int idx = bookingAt(pos);
    QRectF slot;
    int hallId;
    QDateTime start;
    if (idx < 0 && slotAt(pos, &hallId, &start)) {
        const int col = int((pos.x() - TimeColumn) / columnWidth());
        const double y = yForTime(start.time());
        slot = QRectF(TimeColumn + col * columnWidth() + 4, y + 1, columnWidth() - 8, SlotHeight * 2 - 2);
    }

    if (idx != m_hoverBooking || slot != m_hoverSlot) {
        m_hoverBooking = idx;
        m_hoverSlot = slot;
        update();
    }
    setCursor(idx >= 0 || !slot.isNull() ? Qt::PointingHandCursor : Qt::ArrowCursor);

    if (idx >= 0) {
        const Booking &b = m_bookings[idx];
        QToolTip::showText(globalPos,
            QStringLiteral("<b>%1</b><br>%2–%3<br>Статус: %4<br>Сума: %5<br><i>Подвійний клік - відкрити</i>")
                .arg(b.client.toHtmlEscaped(), b.from.time().toString("HH:mm"), b.to.time().toString("HH:mm"),
                     Ui::bookingStatusText(b.status), Exporter::money(b.total)), this);
    } else {
        QToolTip::hideText();
    }
}

void ScheduleWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
#else
    const QPointF pos = event->localPos();
#endif
    int hallId;
    QDateTime start;
    if (bookingAt(pos) < 0 && slotAt(pos, &hallId, &start))
        emit emptySlotClicked(hallId, start);
}

void ScheduleWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
#else
    const QPointF pos = event->localPos();
#endif
    const int idx = bookingAt(pos);
    if (idx >= 0)
        emit bookingActivated(m_bookings[idx].id);
}

void ScheduleWidget::leaveEvent(QEvent *)
{
    m_hoverBooking = -1;
    m_hoverSlot = QRectF();
    unsetCursor();
    update();
}
