#include "dashboardpage.h"
#include "barchart.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/session.h"
#include "theme.h"
#include "widgets.h"

#include <QDate>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

static QTableWidget *smallTable(const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setShowGrid(false);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(34);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->setItemDelegateForColumn(headers.size() - 1, new StatusDelegate(t));
    return t;
}

static QWidget *card(const QString &title, QWidget *content, QWidget *action = nullptr)
{
    auto *frame = new QFrame;
    frame->setObjectName("card");
    auto *head = new QHBoxLayout;
    auto *label = new QLabel(title);
    label->setObjectName("cardHeader");
    head->addWidget(label, 1);
    if (action)
        head->addWidget(action);
    auto *lay = new QVBoxLayout(frame);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->addLayout(head);
    lay->addWidget(content, 1);
    return frame;
}

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent)
{
    m_greeting = new QLabel;
    m_greeting->setObjectName("pageTitle");
    auto *date = new QLabel;
    date->setObjectName("pageSubtitle");
    QLocale ua(QLocale::Ukrainian, QLocale::Ukraine);
    date->setText(ua.toString(QDate::currentDate(), "dddd, d MMMM yyyy"));

    auto *btnBooking = new QPushButton(Theme::icon("plus", Qt::white), QStringLiteral("Нове бронювання"));
    btnBooking->setObjectName("primary");
    auto *btnRental = new QPushButton(Theme::icon("box"), QStringLiteral("Видати реквізит"));
    connect(btnBooking, &QPushButton::clicked, this, &DashboardPage::newBooking);
    connect(btnRental, &QPushButton::clicked, this, &DashboardPage::newRental);

    auto *head = new QHBoxLayout;
    auto *headText = new QVBoxLayout;
    headText->setSpacing(2);
    headText->addWidget(m_greeting);
    headText->addWidget(date);
    head->addLayout(headText, 1);
    head->addWidget(btnRental, 0, Qt::AlignBottom);
    head->addWidget(btnBooking, 0, Qt::AlignBottom);

    m_today = new StatCard(QStringLiteral("Бронювань сьогодні"), "calendar", Theme::Accent);
    m_revenue = new StatCard(QStringLiteral("Виручка за місяць"), "wallet", Theme::Success);
    m_inRent = new StatCard(QStringLiteral("Реквізит в оренді"), "box", Theme::Warning);
    m_overdue = new StatCard(QStringLiteral("Прострочені повернення"), "alert", Theme::Danger);

    auto *cards = new QHBoxLayout;
    cards->setSpacing(14);
    for (StatCard *c : {m_today, m_revenue, m_inRent, m_overdue})
        cards->addWidget(c);

    m_chart = new BarChart;
    m_chart->setValueSuffix(QStringLiteral("грн"));

    m_todayTable = smallTable({QStringLiteral("Час"), QStringLiteral("Зал"), QStringLiteral("Клієнт"),
                               QStringLiteral("Статус")});
    m_returnsTable = smallTable({QStringLiteral("Повернути до"), QStringLiteral("Клієнт"),
                                 QStringLiteral("Реквізит"), QStringLiteral("Статус")});

    auto *toSchedule = new QPushButton(QStringLiteral("Розклад →"));
    toSchedule->setObjectName("link");
    connect(toSchedule, &QPushButton::clicked, this, &DashboardPage::openSchedule);
    auto *toRentals = new QPushButton(QStringLiteral("Уся оренда →"));
    toRentals->setObjectName("link");
    connect(toRentals, &QPushButton::clicked, this, &DashboardPage::openRentals);

    auto *grid = new QGridLayout;
    grid->setSpacing(14);
    grid->addWidget(card(QStringLiteral("Виручка за останні 14 днів"), m_chart), 0, 0, 1, 2);
    grid->addWidget(card(QStringLiteral("Сьогодні у студії"), m_todayTable, toSchedule), 1, 0);
    grid->addWidget(card(QStringLiteral("Найближчі повернення реквізиту"), m_returnsTable, toRentals), 1, 1);
    grid->setRowStretch(1, 1);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(16);
    lay->addLayout(head);
    lay->addLayout(cards);
    lay->addLayout(grid, 1);
}

void DashboardPage::refresh()
{
    const QTime now = QTime::currentTime();
    QString hello = now.hour() < 12 ? QStringLiteral("Доброго ранку")
                  : now.hour() < 18 ? QStringLiteral("Добрий день") : QStringLiteral("Добрий вечір");
    m_greeting->setText(hello + ", " + Session::instance().fullName.section(' ', 0, 0));

    const QDate today = QDate::currentDate();
    const QString todayStr = Db::toDb(today);
    const QDate monthStart(today.year(), today.month(), 1);

    const int todayCount = Db::scalar("SELECT COUNT(*) FROM bookings WHERE substr(start_at,1,10) = ?"
                                      " AND status <> 'cancelled'", {todayStr}).toInt();
    const int newCount = Db::scalar("SELECT COUNT(*) FROM bookings WHERE status = 'new'"
                                    " AND start_at >= ?", {todayStr}).toInt();
    m_today->setValue(QString::number(todayCount),
                      newCount ? QStringLiteral("%1 нових очікують підтвердження").arg(newCount) : QString());

    // виручка - завершені бронювання та повернена оренда разом зі штрафами
    const qint64 bookingsSum = Db::scalar("SELECT COALESCE(SUM(total),0) FROM bookings WHERE status = 'completed'"
                                          " AND start_at >= ?", {Db::toDb(monthStart)}).toLongLong();
    const qint64 rentalsSum = Db::scalar("SELECT COALESCE(SUM(total + fine),0) FROM rentals WHERE status = 'returned'"
                                         " AND start_date >= ?", {Db::toDb(monthStart)}).toLongLong();
    m_revenue->setValue(Exporter::money(bookingsSum + rentalsSum),
                        QStringLiteral("бронювання %1").arg(Exporter::money(bookingsSum)));

    const int units = Db::scalar("SELECT COALESCE(SUM(ri.qty),0) FROM rental_items ri JOIN rentals r ON r.id = ri.rental_id"
                                 " WHERE r.status = 'active' AND r.start_date <= ?", {todayStr}).toInt();
    const int activeRentals = Db::scalar("SELECT COUNT(*) FROM rentals WHERE status = 'active' AND start_date <= ?",
                                         {todayStr}).toInt();
    m_inRent->setValue(QStringLiteral("%1 од.").arg(units), QStringLiteral("%1 активних оренд").arg(activeRentals));

    const int overdue = Db::scalar("SELECT COUNT(*) FROM rentals WHERE status = 'active' AND end_date < ?",
                                   {todayStr}).toInt();
    m_overdue->setValue(QString::number(overdue),
                        overdue ? QStringLiteral("потрібно зв'язатися з клієнтом") : QStringLiteral("усе вчасно"));

    // графік виручки по днях
    QVector<BarChart::Bar> bars;
    QSqlQuery q;
    q.prepare("SELECT substr(start_at,1,10) d, SUM(total) FROM bookings WHERE status = 'completed'"
              " AND start_at >= ? AND start_at < ? GROUP BY d");
    q.addBindValue(Db::toDb(today.addDays(-13)));
    q.addBindValue(Db::toDb(today.addDays(1)));
    QHash<QString, qint64> byDay;
    if (q.exec()) {
        while (q.next())
            byDay[q.value(0).toString()] = q.value(1).toLongLong();
    }
    for (int i = 13; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        BarChart::Bar b;
        b.label = d.toString("dd.MM");
        b.value = byDay.value(Db::toDb(d)) / 100.0;
        if (d.dayOfWeek() >= 6)
            b.color = QColor("#7b9cff");
        bars.append(b);
    }
    m_chart->setBars(bars);

    // сьогоднішні бронювання
    m_todayTable->setRowCount(0);
    q.prepare("SELECT b.start_at, b.end_at, h.name, c.full_name, b.status FROM bookings b"
              " JOIN halls h ON h.id = b.hall_id JOIN clients c ON c.id = b.client_id"
              " WHERE substr(b.start_at,1,10) = ? AND b.status <> 'cancelled' ORDER BY b.start_at");
    q.addBindValue(todayStr);
    if (q.exec()) {
        while (q.next()) {
            const int r = m_todayTable->rowCount();
            m_todayTable->insertRow(r);
            m_todayTable->setItem(r, 0, new QTableWidgetItem(Db::dtFromDb(q.value(0)).toString("HH:mm") + "–"
                                                             + Db::dtFromDb(q.value(1)).toString("HH:mm")));
            m_todayTable->setItem(r, 1, new QTableWidgetItem(q.value(2).toString()));
            m_todayTable->setItem(r, 2, new QTableWidgetItem(q.value(3).toString()));
            m_todayTable->setItem(r, 3, new QTableWidgetItem(Ui::bookingStatusText(q.value(4).toString())));
        }
    }
    m_todayTable->resizeColumnsToContents();

    // активна оренда, відсортована за датою повернення
    m_returnsTable->setRowCount(0);
    q.prepare("SELECT r.end_date, c.full_name, GROUP_CONCAT(p.name || ' ×' || ri.qty, ', ') FROM rentals r"
              " JOIN clients c ON c.id = r.client_id"
              " JOIN rental_items ri ON ri.rental_id = r.id JOIN props p ON p.id = ri.prop_id"
              " WHERE r.status = 'active' GROUP BY r.id ORDER BY r.end_date LIMIT 8");
    if (q.exec()) {
        while (q.next()) {
            const QDate end = Db::dateFromDb(q.value(0));
            const int r = m_returnsTable->rowCount();
            m_returnsTable->insertRow(r);
            m_returnsTable->setItem(r, 0, new QTableWidgetItem(end.toString("dd.MM.yyyy")));
            m_returnsTable->setItem(r, 1, new QTableWidgetItem(q.value(1).toString()));
            m_returnsTable->setItem(r, 2, new QTableWidgetItem(q.value(2).toString()));
            m_returnsTable->setItem(r, 3, new QTableWidgetItem(Ui::rentalStatusText("active", end < today)));
        }
    }
    m_returnsTable->resizeColumnsToContents();
}
