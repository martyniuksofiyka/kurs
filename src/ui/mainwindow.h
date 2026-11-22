#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;
class DashboardPage;
class SchedulePage;
class BookingsPage;
class RentalsPage;
class CatalogPage;
class ClientsPage;
class ReportsPage;
class UsersPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // true, якщо користувач обрав "Змінити користувача"
    bool relogin() const { return m_relogin; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void switchPage(int row);
    void newBooking();
    void newRental();
    void backupDatabase();
    void resetDemo();
    void studioSettings();
    void changePassword();
    void changeUser();
    void showHelp();
    void showAbout();

private:
    void createMenus();
    void createSidebar();
    void refreshCurrent();

    QListWidget *m_nav;
    QStackedWidget *m_stack;
    QLabel *m_clock;

    DashboardPage *m_dashboard;
    SchedulePage *m_schedule;
    BookingsPage *m_bookings;
    RentalsPage *m_rentals;
    CatalogPage *m_catalog;
    ClientsPage *m_clients;
    ReportsPage *m_reports;
    UsersPage *m_users = nullptr;

    bool m_relogin = false;
};

#endif // MAINWINDOW_H
