#include "mainwindow.h"
#include "bookingspage.h"
#include "catalogpage.h"
#include "clientspage.h"
#include "core/database.h"
#include "core/session.h"
#include "dashboardpage.h"
#include "helpdialog.h"
#include "rentalspage.h"
#include "reportspage.h"
#include "schedulepage.h"
#include "theme.h"
#include "userspage.h"
#include "widgets.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimeEdit>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(Session::instance().studioName() + QStringLiteral(" — бронювання та оренда"));
    setWindowIcon(Theme::icon("camera", Theme::Accent));
    setMinimumSize(1180, 720);

    m_stack = new QStackedWidget;
    m_dashboard = new DashboardPage;
    m_schedule = new SchedulePage;
    m_bookings = new BookingsPage;
    m_rentals = new RentalsPage;
    m_catalog = new CatalogPage;
    m_clients = new ClientsPage;
    m_reports = new ReportsPage;

    m_stack->addWidget(m_dashboard);
    m_stack->addWidget(m_schedule);
    m_stack->addWidget(m_bookings);
    m_stack->addWidget(m_rentals);
    m_stack->addWidget(m_clients);
    m_stack->addWidget(m_catalog);
    m_stack->addWidget(m_reports);
    if (Session::instance().isAdmin()) {
        m_users = new UsersPage;
        m_stack->addWidget(m_users);
    }

    createSidebar();
    createMenus();

    auto *central = new QWidget;
    auto *lay = new QHBoxLayout(central);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_nav);
    lay->addWidget(m_stack, 1);
    setCentralWidget(central);

    // з головної сторінки можна перейти до потрібного розділу
    connect(m_dashboard, &DashboardPage::openSchedule, this, [this] { m_nav->setCurrentRow(1); });
    connect(m_dashboard, &DashboardPage::openRentals, this, [this] { m_nav->setCurrentRow(3); });
    connect(m_dashboard, &DashboardPage::newBooking, this, &MainWindow::newBooking);
    connect(m_dashboard, &DashboardPage::newRental, this, &MainWindow::newRental);

    auto *user = new QLabel(QStringLiteral("Користувач: %1 (%2)")
                                .arg(Session::instance().fullName,
                                     Session::instance().isAdmin() ? QStringLiteral("адміністратор")
                                                                   : QStringLiteral("менеджер")));
    m_clock = new QLabel;
    statusBar()->addWidget(user);
    statusBar()->addPermanentWidget(m_clock);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        m_clock->setText(QDateTime::currentDateTime().toString("dd.MM.yyyy  HH:mm"));
    });
    timer->start(1000);
    m_clock->setText(QDateTime::currentDateTime().toString("dd.MM.yyyy  HH:mm"));

    QSettings st;
    restoreGeometry(st.value("window/geometry").toByteArray());
    m_nav->setCurrentRow(0);
}

void MainWindow::createSidebar()
{
    m_nav = new QListWidget;
    m_nav->setObjectName("sidebar");
    m_nav->setFixedWidth(220);
    m_nav->setIconSize(QSize(20, 20));
    m_nav->setFocusPolicy(Qt::NoFocus);

    struct Item { const char *text; const char *icon; };
    const Item items[] = {
        {"Головна", "home"},
        {"Розклад", "calendar"},
        {"Бронювання", "booking"},
        {"Оренда реквізиту", "box"},
        {"Клієнти", "users"},
        {"Зали та реквізит", "camera"},
        {"Звіти", "chart"},
    };
    for (const Item &i : items)
        new QListWidgetItem(Theme::icon(i.icon, QColor("#aab2c5")), QString::fromUtf8(i.text), m_nav);
    if (m_users)
        new QListWidgetItem(Theme::icon("shield", QColor("#aab2c5")), QStringLiteral("Користувачі"), m_nav);

    connect(m_nav, &QListWidget::currentRowChanged, this, &MainWindow::switchPage);
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(QStringLiteral("&Файл"));
    QAction *a = file->addAction(Theme::icon("booking"), QStringLiteral("Нове бронювання..."), this, &MainWindow::newBooking);
    a->setShortcut(QKeySequence::New);
    a = file->addAction(Theme::icon("box"), QStringLiteral("Нова оренда реквізиту..."), this, &MainWindow::newRental);
    a->setShortcut(QKeySequence("Ctrl+Shift+N"));
    file->addSeparator();
    file->addAction(Theme::icon("save"), QStringLiteral("Резервна копія бази даних..."), this, &MainWindow::backupDatabase);
    if (Session::instance().isAdmin())
        file->addAction(QStringLiteral("Відновити навчальні дані..."), this, &MainWindow::resetDemo);
    file->addSeparator();
    file->addAction(QStringLiteral("Змінити користувача"), this, &MainWindow::changeUser);
    a = file->addAction(QStringLiteral("Вихід"), this, &QWidget::close);
    a->setShortcut(QKeySequence("Ctrl+Q"));

    QMenu *view = menuBar()->addMenu(QStringLiteral("&Вигляд"));
    a = view->addAction(QStringLiteral("Оновити дані"), this, &MainWindow::refreshCurrent);
    a->setShortcut(QKeySequence::Refresh);
    view->addSeparator();
    for (int i = 0; i < 7; ++i) {
        a = view->addAction(m_nav->item(i)->text(), this, [this, i] { m_nav->setCurrentRow(i); });
        a->setShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)));
    }

    QMenu *settings = menuBar()->addMenu(QStringLiteral("&Налаштування"));
    QAction *studio = settings->addAction(QStringLiteral("Параметри студії..."), this, &MainWindow::studioSettings);
    studio->setEnabled(Session::instance().isAdmin());
    settings->addAction(QStringLiteral("Змінити пароль..."), this, &MainWindow::changePassword);

    QMenu *help = menuBar()->addMenu(QStringLiteral("&Довідка"));
    a = help->addAction(Theme::icon("help"), QStringLiteral("Допомога"), this, &MainWindow::showHelp);
    a->setShortcut(QKeySequence::HelpContents);
    help->addAction(QStringLiteral("Про програму"), this, &MainWindow::showAbout);
}

void MainWindow::switchPage(int row)
{
    if (row < 0)
        return;
    m_stack->setCurrentIndex(row);
    refreshCurrent();
}

void MainWindow::refreshCurrent()
{
    // кожна сторінка має слот refresh(), викликаємо його через мета-систему Qt
    QMetaObject::invokeMethod(m_stack->currentWidget(), "refresh");
}

void MainWindow::newBooking()
{
    m_nav->setCurrentRow(2);
    m_bookings->addBooking();
}

void MainWindow::newRental()
{
    m_nav->setCurrentRow(3);
    m_rentals->addRental();
}

void MainWindow::backupDatabase()
{
    const QString name = QFileDialog::getSaveFileName(
        this, QStringLiteral("Резервна копія"),
        QString("photostudio_%1.db").arg(QDate::currentDate().toString("yyyy-MM-dd")),
        QStringLiteral("База даних SQLite (*.db)"));
    if (name.isEmpty())
        return;
    QString error;
    if (Db::backup(name, &error))
        QMessageBox::information(this, QStringLiteral("Резервна копія"),
                                 QStringLiteral("Копію бази даних збережено:\n%1").arg(name));
    else
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
}

void MainWindow::resetDemo()
{
    if (!Ui::confirm(this, QStringLiteral("Навчальні дані"),
                     QStringLiteral("Усі поточні дані буде видалено і замінено навчальним набором.\n"
                                    "Рекомендуємо спочатку зробити резервну копію. Продовжити?")))
        return;
    QString error;
    if (!Db::resetDemoData(&error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Навчальні дані"),
                             QStringLiteral("Навчальні дані відновлено. Увійдіть у систему ще раз."));
    changeUser();
}

void MainWindow::studioSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Параметри студії"));
    auto *name = new QLineEdit(Session::instance().studioName());
    auto *open = new QTimeEdit(Session::instance().openTime());
    auto *close = new QTimeEdit(Session::instance().closeTime());
    open->setDisplayFormat("HH:mm");
    close->setDisplayFormat("HH:mm");

    auto *form = new QFormLayout(&dlg);
    form->addRow(QStringLiteral("Назва студії:"), name);
    form->addRow(QStringLiteral("Початок роботи:"), open);
    form->addRow(QStringLiteral("Кінець роботи:"), close);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;
    if (open->time() >= close->time()) {
        QMessageBox::warning(this, QStringLiteral("Помилка"),
                             QStringLiteral("Час початку роботи має бути раніше за час завершення."));
        return;
    }
    if (!name->text().trimmed().isEmpty())
        Session::instance().setStudioName(name->text().trimmed());
    Session::instance().setWorkingHours(open->time(), close->time());
    setWindowTitle(Session::instance().studioName() + QStringLiteral(" — бронювання та оренда"));
    refreshCurrent();
}

void MainWindow::changePassword()
{
    bool ok = false;
    const QString oldPass = QInputDialog::getText(this, QStringLiteral("Зміна пароля"),
                                                  QStringLiteral("Поточний пароль:"), QLineEdit::Password, {}, &ok);
    if (!ok)
        return;
    if (!Db::authenticate(Session::instance().login, oldPass)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), QStringLiteral("Поточний пароль введено невірно."));
        return;
    }
    const QString newPass = QInputDialog::getText(this, QStringLiteral("Зміна пароля"),
                                                  QStringLiteral("Новий пароль (не менше 6 символів):"),
                                                  QLineEdit::Password, {}, &ok);
    if (!ok)
        return;
    if (newPass.length() < 6) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), QStringLiteral("Пароль занадто короткий."));
        return;
    }
    Db::setPassword(Session::instance().userId, newPass);
    QMessageBox::information(this, QStringLiteral("Зміна пароля"), QStringLiteral("Пароль змінено."));
}

void MainWindow::changeUser()
{
    m_relogin = true;
    close();
}

void MainWindow::showHelp()
{
    HelpDialog dlg(this);
    dlg.exec();
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Про програму"));
    box.setIconPixmap(Theme::icon("camera", Theme::Accent).pixmap(64, 64));
    box.setText(QStringLiteral("<h3>PhotoStudio Manager</h3>"
                               "<p>Версія %1</p>").arg(QApplication::applicationVersion()));
    box.setInformativeText(QStringLiteral(
        "<p>Курсова робота на тему:<br><b>«Розробка програмної системи з автоматизації "
        "бронювання фотостудії та оренди реквізиту»</b></p>"
        "<p>Автор: студент групи <b>[ГРУПА]</b> <b>[ПІБ]</b><br>"
        "Керівник: <b>[КЕРІВНИК]</b><br>"
        "Рік: 2026</p>"
        "<p>C++17, Qt %1, SQLite</p>"
        "<p>База даних: %2</p>").arg(QString::fromLatin1(qVersion()), Db::path().toHtmlEscaped()));
    box.exec();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings().setValue("window/geometry", saveGeometry());
    event->accept();
}
