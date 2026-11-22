#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>

class BarChart;
class QLabel;
class QTableWidget;
class StatCard;

// Головна сторінка: ключові показники за сьогодні та місяць
class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);

public slots:
    void refresh();

signals:
    void openSchedule();
    void openRentals();
    void newBooking();
    void newRental();

private:
    QLabel *m_greeting;
    StatCard *m_today;
    StatCard *m_revenue;
    StatCard *m_inRent;
    StatCard *m_overdue;
    BarChart *m_chart;
    QTableWidget *m_todayTable;
    QTableWidget *m_returnsTable;
};

#endif // DASHBOARDPAGE_H
