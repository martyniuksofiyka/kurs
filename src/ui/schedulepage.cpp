#include "schedulepage.h"
#include "bookingdialog.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/pricing.h"
#include "core/session.h"
#include "schedulewidget.h"
#include "theme.h"
#include "widgets.h"

#include <QCalendarWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlQuery>
#include <QTextCharFormat>
#include <QVBoxLayout>

SchedulePage::SchedulePage(QWidget *parent)
    : QWidget(parent)
{
    m_calendar = new QCalendarWidget;
    m_calendar->setGridVisible(false);
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_calendar->setFirstDayOfWeek(Qt::Monday);
    m_calendar->setLocale(QLocale(QLocale::Ukrainian, QLocale::Ukraine));
    m_calendar->setFixedSize(280, 250);
    connect(m_calendar, &QCalendarWidget::selectionChanged, this, &SchedulePage::refresh);
    connect(m_calendar, &QCalendarWidget::currentPageChanged, this, [this] { markBusyDays(); });

    auto *btnToday = new QPushButton(QStringLiteral("Сьогодні"));
    auto *btnPrev = new QPushButton(Theme::icon("left"), QString());
    auto *btnNext = new QPushButton(Theme::icon("right"), QString());
    btnPrev->setToolTip(QStringLiteral("Попередній день"));
    btnNext->setToolTip(QStringLiteral("Наступний день"));
    connect(btnToday, &QPushButton::clicked, this, [this] { m_calendar->setSelectedDate(QDate::currentDate()); });
    connect(btnPrev, &QPushButton::clicked, this, [this] { m_calendar->setSelectedDate(m_calendar->selectedDate().addDays(-1)); });
    connect(btnNext, &QPushButton::clicked, this, [this] { m_calendar->setSelectedDate(m_calendar->selectedDate().addDays(1)); });

    auto *nav = new QHBoxLayout;
    nav->addWidget(btnPrev);
    nav->addWidget(btnToday, 1);
    nav->addWidget(btnNext);

    m_summary = new QLabel;
    m_summary->setObjectName("summary");
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);

    auto *legend = new QLabel(QStringLiteral(
        "<b>Як користуватися</b><br>"
        "• клік по вільній клітинці - нове бронювання;<br>"
        "• подвійний клік по блоку - відкрити бронювання;<br>"
        "• заштриховані блоки ще не підтверджені;<br>"
        "• жовтуватий фон - вихідний день (+20 %)."));
    legend->setObjectName("hint");
    legend->setWordWrap(true);

    auto *side = new QVBoxLayout;
    side->addWidget(m_calendar);
    side->addLayout(nav);
    side->addSpacing(8);
    side->addWidget(m_summary);
    side->addStretch();
    side->addWidget(legend);

    m_dayTitle = new QLabel;
    m_dayTitle->setObjectName("pageTitle");

    m_schedule = new ScheduleWidget;
    connect(m_schedule, &ScheduleWidget::emptySlotClicked, this, &SchedulePage::createAt);
    connect(m_schedule, &ScheduleWidget::bookingActivated, this, &SchedulePage::openBooking);

    auto *scroll = new QScrollArea;
    scroll->setObjectName("scheduleScroll");
    scroll->setWidget(m_schedule);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *frame = new QFrame;
    frame->setObjectName("card");
    auto *frameLay = new QVBoxLayout(frame);
    frameLay->setContentsMargins(0, 8, 0, 0);
    frameLay->addWidget(scroll);

    auto *main = new QVBoxLayout;
    main->addWidget(m_dayTitle);
    main->addWidget(frame, 1);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(18);
    lay->addLayout(side);
    lay->addLayout(main, 1);
}

void SchedulePage::refresh()
{
    const QDate date = m_calendar->selectedDate();
    QLocale ua(QLocale::Ukrainian, QLocale::Ukraine);
    QString title = ua.toString(date, "dddd, d MMMM yyyy");
    title[0] = title[0].toUpper();
    m_dayTitle->setText(title);

    QVector<ScheduleWidget::Hall> halls;
    QSqlQuery q("SELECT id, name, color FROM halls WHERE active = 1 ORDER BY id");
    while (q.next())
        halls.append({q.value(0).toInt(), q.value(1).toString(), QColor(q.value(2).toString())});

    QVector<ScheduleWidget::Booking> bookings;
    q.prepare("SELECT b.id, b.hall_id, b.start_at, b.end_at, c.full_name, b.status, b.total FROM bookings b"
              " JOIN clients c ON c.id = b.client_id"
              " WHERE substr(b.start_at,1,10) = ? AND b.status <> 'cancelled' ORDER BY b.start_at");
    q.addBindValue(Db::toDb(date));
    qint64 sum = 0;
    int minutes = 0;
    if (q.exec()) {
        while (q.next()) {
            ScheduleWidget::Booking b{q.value(0).toInt(), q.value(1).toInt(), Db::dtFromDb(q.value(2)),
                                      Db::dtFromDb(q.value(3)), q.value(4).toString(), q.value(5).toString(),
                                      q.value(6).toLongLong()};
            sum += b.total;
            minutes += int(b.from.secsTo(b.to) / 60);
            bookings.append(b);
        }
    }

    const Session &s = Session::instance();
    m_schedule->setData(date, halls, bookings, s.openTime(), s.closeTime());

    // завантаженість = заброньовані години / (кількість залів × робочі години)
    const int dayMinutes = int(s.openTime().secsTo(s.closeTime()) / 60) * qMax(1, halls.size());
    const int load = dayMinutes > 0 ? qRound(minutes * 100.0 / dayMinutes) : 0;
    m_summary->setText(QStringLiteral("<b>Підсумок дня</b><br>Бронювань: %1<br>Зайнято: %2 год (%3 %)<br>"
                                      "Сума: %4%5")
                           .arg(bookings.size())
                           .arg(minutes / 60.0, 0, 'f', 1)
                           .arg(load)
                           .arg(Exporter::money(sum))
                           .arg(Pricing::isWeekend(date) ? QStringLiteral("<br><i>Вихідний: діє надбавка 20 %</i>")
                                                         : QString()));
    markBusyDays();
}

void SchedulePage::markBusyDays()
{
    // дні з бронюваннями виділяємо жирним шрифтом
    const int year = m_calendar->yearShown();
    const int month = m_calendar->monthShown();
    const QDate first(year, month, 1);
    m_calendar->setDateTextFormat(QDate(), QTextCharFormat());

    QSqlQuery q;
    q.prepare("SELECT DISTINCT substr(start_at,1,10) FROM bookings WHERE status <> 'cancelled'"
              " AND start_at >= ? AND start_at < ?");
    q.addBindValue(Db::toDb(first));
    q.addBindValue(Db::toDb(first.addMonths(1)));
    QTextCharFormat busy;
    busy.setFontWeight(QFont::Bold);
    busy.setForeground(Theme::Accent);
    if (q.exec()) {
        while (q.next())
            m_calendar->setDateTextFormat(Db::dateFromDb(q.value(0)), busy);
    }
}

void SchedulePage::createAt(int hallId, const QDateTime &start)
{
    if (start.addSecs(3600) < QDateTime::currentDateTime().addSecs(-3600)) {
        if (!Ui::confirm(this, QStringLiteral("Минула дата"),
                         QStringLiteral("Обраний час уже минув. Все одно створити бронювання?")))
            return;
    }
    BookingDialog dlg(0, this);
    dlg.preset(hallId, start);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void SchedulePage::openBooking(int bookingId)
{
    BookingDialog dlg(bookingId, this);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}
