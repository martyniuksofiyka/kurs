#ifndef SCHEDULEPAGE_H
#define SCHEDULEPAGE_H

#include <QWidget>

class QCalendarWidget;
class QLabel;
class ScheduleWidget;

class SchedulePage : public QWidget
{
    Q_OBJECT
public:
    explicit SchedulePage(QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void createAt(int hallId, const QDateTime &start);
    void openBooking(int bookingId);

private:
    void markBusyDays();

    QCalendarWidget *m_calendar;
    ScheduleWidget *m_schedule;
    QLabel *m_dayTitle;
    QLabel *m_summary;
};

#endif // SCHEDULEPAGE_H
