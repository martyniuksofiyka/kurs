#ifndef SCHEDULEWIDGET_H
#define SCHEDULEWIDGET_H

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QVector>
#include <QWidget>

// Сітка розкладу на один день: стовпці - зали, рядки - час.
// Бронювання відображаються кольоровими блоками.
class ScheduleWidget : public QWidget
{
    Q_OBJECT
public:
    struct Hall {
        int id;
        QString name;
        QColor color;
    };
    struct Booking {
        int id;
        int hallId;
        QDateTime from;
        QDateTime to;
        QString client;
        QString status;
        qint64 total;   // у копійках
    };

    explicit ScheduleWidget(QWidget *parent = nullptr);

    void setData(const QDate &date, const QVector<Hall> &halls, const QVector<Booking> &bookings,
                 const QTime &open, const QTime &close);
    QSize sizeHint() const override;

signals:
    // клік по вільному місцю - створити бронювання з цим залом і часом
    void emptySlotClicked(int hallId, const QDateTime &start);
    void bookingActivated(int bookingId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    static constexpr int HeaderHeight = 44;
    static constexpr int TimeColumn = 62;
    static constexpr int SlotHeight = 28;   // висота півгодинного інтервалу

    int slotCount() const;
    double columnWidth() const;
    double yForTime(const QTime &t) const;
    QRectF bookingRect(const Booking &b) const;
    int bookingAt(const QPointF &pos) const;
    // зал та час, що відповідають точці на сітці
    bool slotAt(const QPointF &pos, int *hallId, QDateTime *start) const;

    QDate m_date;
    QTime m_open = QTime(8, 0);
    QTime m_close = QTime(22, 0);
    QVector<Hall> m_halls;
    QVector<Booking> m_bookings;
    int m_hoverBooking = -1;
    QRectF m_hoverSlot;
};

#endif // SCHEDULEWIDGET_H
