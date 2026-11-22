#include "reportspage.h"
#include "barchart.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/pricing.h"
#include "core/session.h"
#include "theme.h"
#include "widgets.h"

#include <QBuffer>
#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

ReportsPage::ReportsPage(QWidget *parent)
    : QWidget(parent)
{
    m_type = new QComboBox;
    m_type->addItems({QStringLiteral("Виручка та завантаженість залів"), QStringLiteral("Популярність реквізиту"),
                      QStringLiteral("Найактивніші клієнти"), QStringLiteral("Виручка по днях"),
                      QStringLiteral("Завантаженість за днями тижня")});

    const QDate today = QDate::currentDate();
    m_from = new QDateEdit(QDate(today.year(), today.month(), 1));
    m_to = new QDateEdit(today);
    for (QDateEdit *d : {m_from, m_to}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd.MM.yyyy");
        connect(d, &QDateEdit::dateChanged, this, &ReportsPage::refresh);
    }
    connect(m_type, QOverload<int>::of(&QComboBox::activated), this, &ReportsPage::refresh);

    // швидкий вибір періоду
    auto *period = new QComboBox;
    period->addItems({QStringLiteral("Цей місяць"), QStringLiteral("Минулий місяць"),
                      QStringLiteral("Останні 7 днів"), QStringLiteral("Останні 30 днів"), QStringLiteral("Цей рік")});
    connect(period, QOverload<int>::of(&QComboBox::activated), this, [this](int i) {
        const QDate t = QDate::currentDate();
        QDate from = t, to = t;
        switch (i) {
        case 0: from = QDate(t.year(), t.month(), 1); break;
        case 1: from = QDate(t.year(), t.month(), 1).addMonths(-1); to = QDate(t.year(), t.month(), 1).addDays(-1); break;
        case 2: from = t.addDays(-6); break;
        case 3: from = t.addDays(-29); break;
        case 4: from = QDate(t.year(), 1, 1); break;
        }
        m_from->blockSignals(true);
        m_from->setDate(from);
        m_from->blockSignals(false);
        m_to->setDate(to);
        refresh();
    });

    auto *btnPdf = new QPushButton(Theme::icon("file"), QStringLiteral("PDF"));
    auto *btnCsv = new QPushButton(Theme::icon("download"), QStringLiteral("CSV"));
    auto *btnPrint = new QPushButton(Theme::icon("print"), QStringLiteral("Друк"));
    connect(btnPdf, &QPushButton::clicked, this, &ReportsPage::savePdf);
    connect(btnCsv, &QPushButton::clicked, this, &ReportsPage::saveCsv);
    connect(btnPrint, &QPushButton::clicked, this, &ReportsPage::print);

    auto *bar = new QHBoxLayout;
    bar->addWidget(m_type, 2);
    bar->addWidget(period);
    bar->addWidget(new QLabel(QStringLiteral("з")));
    bar->addWidget(m_from);
    bar->addWidget(new QLabel(QStringLiteral("по")));
    bar->addWidget(m_to);
    bar->addStretch();
    bar->addWidget(btnPdf);
    bar->addWidget(btnCsv);
    bar->addWidget(btnPrint);

    m_chart = new BarChart;
    m_chart->setMinimumHeight(240);
    auto *chartCard = new QFrame;
    chartCard->setObjectName("card");
    auto *cl = new QVBoxLayout(chartCard);
    cl->addWidget(m_chart);

    m_model = new QStandardItemModel(this);
    auto *proxy = new TableProxy(this);
    proxy->setSourceModel(m_model);
    m_view = new QTableView;
    m_view->setModel(proxy);
    Ui::setupTable(m_view);

    m_summary = new QLabel;
    m_summary->setObjectName("summary");
    m_summary->setTextFormat(Qt::RichText);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);
    lay->addWidget(Ui::pageHeader(QStringLiteral("Звіти"), QStringLiteral("Аналітика роботи студії за обраний період")));
    lay->addLayout(bar);
    lay->addWidget(chartCard);
    lay->addWidget(m_view, 1);
    lay->addWidget(m_summary);
}

void ReportsPage::refresh()
{
    if (m_from->date() > m_to->date()) {
        m_summary->setText(QStringLiteral("<span style='color:#d9534f'>Початок періоду пізніше за кінець.</span>"));
        return;
    }
    m_model->clear();
    switch (m_type->currentIndex()) {
    case 0: reportHalls(); break;
    case 1: reportProps(); break;
    case 2: reportClients(); break;
    case 3: reportDays(); break;
    case 4: reportWeekdays(); break;
    }
    m_view->resizeColumnsToContents();
}

void ReportsPage::reportHalls()
{
    m_model->setHorizontalHeaderLabels({QStringLiteral("Зал"), QStringLiteral("Бронювань"), QStringLiteral("Годин"),
                                        QStringLiteral("Завантаженість"), QStringLiteral("Виручка"),
                                        QStringLiteral("Середній чек")});
    const int days = int(m_from->date().daysTo(m_to->date())) + 1;
    const double workHours = Session::instance().openTime().secsTo(Session::instance().closeTime()) / 3600.0;

    // LEFT JOIN, щоб у звіті були й зали без бронювань
    QSqlQuery q;
    q.prepare("SELECT h.name, h.color, COUNT(b.id),"
              " COALESCE(SUM((julianday(b.end_at) - julianday(b.start_at)) * 24), 0),"
              " COALESCE(SUM(CASE WHEN b.status = 'completed' THEN b.total ELSE 0 END), 0)"
              " FROM halls h LEFT JOIN bookings b ON b.hall_id = h.id AND b.status <> 'cancelled'"
              " AND b.start_at >= ? AND b.start_at < ?"
              " GROUP BY h.id ORDER BY 5 DESC");
    q.addBindValue(Db::toDb(m_from->date()));
    q.addBindValue(Db::toDb(m_to->date().addDays(1)));

    QVector<BarChart::Bar> bars;
    qint64 total = 0;
    int count = 0;
    if (q.exec()) {
        while (q.next()) {
            const int n = q.value(2).toInt();
            const double hours = q.value(3).toDouble();
            const qint64 sum = q.value(4).toLongLong();
            const int load = workHours > 0 ? qRound(hours * 100 / (workHours * days)) : 0;
            m_model->appendRow({Ui::cell(q.value(0).toString()), Ui::cell(QString::number(n), n),
                                Ui::cell(QString::number(hours, 'f', 1), hours),
                                Ui::cell(QStringLiteral("%1 %").arg(load), load), Ui::moneyCell(sum),
                                Ui::moneyCell(n ? sum / n : 0)});
            bars.append({q.value(0).toString(), sum / 100.0, QColor(q.value(1).toString())});
            total += sum;
            count += n;
        }
    }
    m_chart->setTitle(QStringLiteral("Виручка за завершеними бронюваннями, грн"));
    m_chart->setValueSuffix(QStringLiteral("грн"));
    m_chart->setBars(bars);
    m_summary->setText(QStringLiteral("Усього бронювань: <b>%1</b>, виручка: <b>%2</b>")
                           .arg(count).arg(Exporter::money(total)));
}

void ReportsPage::reportProps()
{
    m_model->setHorizontalHeaderLabels({QStringLiteral("Реквізит"), QStringLiteral("Категорія"),
                                        QStringLiteral("Разом із залом"), QStringLiteral("Оренда на винос"),
                                        QStringLiteral("Одиниць видано"), QStringLiteral("Дохід")});
    struct Row { QString name, category; int withHall = 0, rentals = 0, units = 0; qint64 income = 0; };
    QMap<int, Row> rows;

    QSqlQuery q("SELECT id, name, category FROM props");
    while (q.next())
        rows[q.value(0).toInt()] = {q.value(1).toString(), q.value(2).toString()};

    const QString from = Db::toDb(m_from->date());
    const QString to = Db::toDb(m_to->date().addDays(1));

    q.prepare("SELECT bp.prop_id, COUNT(*), SUM(bp.qty),"
              " SUM(bp.price * bp.qty * (julianday(b.end_at) - julianday(b.start_at)) * 24)"
              " FROM booking_props bp JOIN bookings b ON b.id = bp.booking_id"
              " WHERE b.status <> 'cancelled' AND b.start_at >= ? AND b.start_at < ? GROUP BY bp.prop_id");
    q.addBindValue(from);
    q.addBindValue(to);
    if (q.exec()) {
        while (q.next()) {
            Row &r = rows[q.value(0).toInt()];
            r.withHall = q.value(1).toInt();
            r.units += q.value(2).toInt();
            r.income += qRound64(q.value(3).toDouble());
        }
    }
    q.prepare("SELECT ri.prop_id, COUNT(*), SUM(ri.qty),"
              " SUM(ri.price_per_day * ri.qty * (julianday(r.end_date) - julianday(r.start_date) + 1))"
              " FROM rental_items ri JOIN rentals r ON r.id = ri.rental_id"
              " WHERE r.status <> 'cancelled' AND r.start_date >= ? AND r.start_date < ? GROUP BY ri.prop_id");
    q.addBindValue(from);
    q.addBindValue(to);
    if (q.exec()) {
        while (q.next()) {
            Row &r = rows[q.value(0).toInt()];
            r.rentals = q.value(1).toInt();
            r.units += q.value(2).toInt();
            r.income += qRound64(q.value(3).toDouble());
        }
    }

    // сортуємо за доходом (за спаданням)
    QList<Row> list = rows.values();
    std::sort(list.begin(), list.end(), [](const Row &a, const Row &b) { return a.income > b.income; });

    QVector<BarChart::Bar> bars;
    qint64 total = 0;
    for (const Row &r : list) {
        m_model->appendRow({Ui::cell(r.name), Ui::cell(r.category), Ui::cell(QString::number(r.withHall), r.withHall),
                            Ui::cell(QString::number(r.rentals), r.rentals), Ui::cell(QString::number(r.units), r.units),
                            Ui::moneyCell(r.income)});
        if (bars.size() < 8 && r.income > 0)
            bars.append({r.name, r.income / 100.0, QColor()});
        total += r.income;
    }
    m_chart->setTitle(QStringLiteral("Топ-8 реквізиту за доходом, грн"));
    m_chart->setValueSuffix(QStringLiteral("грн"));
    m_chart->setColor(Theme::Warning);
    m_chart->setBars(bars);
    m_chart->setColor(Theme::Accent);
    m_summary->setText(QStringLiteral("Дохід від реквізиту за період: <b>%1</b>").arg(Exporter::money(total)));
}

void ReportsPage::reportClients()
{
    m_model->setHorizontalHeaderLabels({QStringLiteral("Клієнт"), QStringLiteral("Телефон"), QStringLiteral("Візитів"),
                                        QStringLiteral("Оренд"), QStringLiteral("Сума")});
    QSqlQuery q;
    // період передаємо один раз через CTE, щоб не дублювати параметри
    q.prepare("WITH p(f, t) AS (SELECT ?, ?)"
              " SELECT c.full_name, c.phone,"
              " (SELECT COUNT(*) FROM bookings b WHERE b.client_id = c.id AND b.status = 'completed'"
              "   AND b.start_at >= p.f AND b.start_at < p.t) AS visits,"
              " (SELECT COUNT(*) FROM rentals r WHERE r.client_id = c.id AND r.status = 'returned'"
              "   AND r.start_date >= p.f AND r.start_date < p.t) AS rents,"
              " (SELECT COALESCE(SUM(total),0) FROM bookings b WHERE b.client_id = c.id AND b.status = 'completed'"
              "   AND b.start_at >= p.f AND b.start_at < p.t)"
              " + (SELECT COALESCE(SUM(total + fine),0) FROM rentals r WHERE r.client_id = c.id AND r.status = 'returned'"
              "   AND r.start_date >= p.f AND r.start_date < p.t) AS spent"
              " FROM clients c, p WHERE visits + rents > 0 ORDER BY spent DESC LIMIT 15");
    q.addBindValue(Db::toDb(m_from->date()));
    q.addBindValue(Db::toDb(m_to->date().addDays(1)));

    QVector<BarChart::Bar> bars;
    if (q.exec()) {
        while (q.next()) {
            m_model->appendRow({Ui::cell(q.value(0).toString()), Ui::cell(q.value(1).toString()),
                                Ui::cell(q.value(2).toString(), q.value(2)), Ui::cell(q.value(3).toString(), q.value(3)),
                                Ui::moneyCell(q.value(4).toLongLong())});
            if (bars.size() < 8)
                bars.append({q.value(0).toString().section(' ', 0, 0), q.value(4).toLongLong() / 100.0, QColor()});
        }
    }
    m_chart->setTitle(QStringLiteral("Найбільші суми замовлень за період, грн"));
    m_chart->setValueSuffix(QStringLiteral("грн"));
    m_chart->setBars(bars);
    m_summary->setText(QStringLiteral("Активних клієнтів за період: <b>%1</b>").arg(m_model->rowCount()));
}

void ReportsPage::reportDays()
{
    m_model->setHorizontalHeaderLabels({QStringLiteral("Дата"), QStringLiteral("День тижня"),
                                        QStringLiteral("Бронювань"), QStringLiteral("Виручка")});
    QSqlQuery q;
    q.prepare("SELECT substr(start_at,1,10) d, COUNT(*), SUM(total) FROM bookings"
              " WHERE status = 'completed' AND start_at >= ? AND start_at < ? GROUP BY d ORDER BY d");
    q.addBindValue(Db::toDb(m_from->date()));
    q.addBindValue(Db::toDb(m_to->date().addDays(1)));

    QLocale ua(QLocale::Ukrainian, QLocale::Ukraine);
    QHash<QString, QPair<int, qint64>> data;
    if (q.exec()) {
        while (q.next())
            data[q.value(0).toString()] = {q.value(1).toInt(), q.value(2).toLongLong()};
    }

    // проходимо всі дні періоду, щоб на графіку були й дні без виручки
    QVector<BarChart::Bar> bars;
    qint64 total = 0;
    int best = 0;
    QString bestDay;
    for (QDate d = m_from->date(); d <= m_to->date(); d = d.addDays(1)) {
        const auto v = data.value(Db::toDb(d));
        m_model->appendRow({Ui::cell(d.toString("dd.MM.yyyy"), Db::toDb(d)), Ui::cell(ua.dayName(d.dayOfWeek())),
                            Ui::cell(QString::number(v.first), v.first), Ui::moneyCell(v.second)});
        bars.append({d.toString("dd.MM"), v.second / 100.0, d.dayOfWeek() >= 6 ? QColor("#7b9cff") : QColor()});
        total += v.second;
        if (v.second > best) {
            best = int(v.second);
            bestDay = d.toString("dd.MM.yyyy");
        }
    }
    const int days = qMax(1, int(m_from->date().daysTo(m_to->date())) + 1);
    m_chart->setTitle(QStringLiteral("Виручка по днях, грн"));
    m_chart->setValueSuffix(QStringLiteral("грн"));
    m_chart->setBars(bars);
    m_summary->setText(QStringLiteral("Разом: <b>%1</b>, у середньому за день: <b>%2</b>%3")
                           .arg(Exporter::money(total), Exporter::money(total / days))
                           .arg(bestDay.isEmpty() ? QString() : QStringLiteral(", найкращий день: <b>%1</b>").arg(bestDay)));
}

void ReportsPage::reportWeekdays()
{
    m_model->setHorizontalHeaderLabels({QStringLiteral("День тижня"), QStringLiteral("Бронювань"),
                                        QStringLiteral("Годин"), QStringLiteral("Виручка")});
    // strftime('%w') повертає 0 для неділі, 1 - для понеділка...
    QSqlQuery q;
    q.prepare("SELECT CAST(strftime('%w', start_at) AS INTEGER), COUNT(*),"
              " SUM((julianday(end_at) - julianday(start_at)) * 24),"
              " SUM(CASE WHEN status = 'completed' THEN total ELSE 0 END)"
              " FROM bookings WHERE status <> 'cancelled' AND start_at >= ? AND start_at < ? GROUP BY 1");
    q.addBindValue(Db::toDb(m_from->date()));
    q.addBindValue(Db::toDb(m_to->date().addDays(1)));

    struct Row { int count = 0; double hours = 0; qint64 sum = 0; };
    Row rows[8];
    if (q.exec()) {
        while (q.next()) {
            int dow = q.value(0).toInt();
            if (dow == 0)
                dow = 7;
            rows[dow] = {q.value(1).toInt(), q.value(2).toDouble(), q.value(3).toLongLong()};
        }
    }
    QLocale ua(QLocale::Ukrainian, QLocale::Ukraine);
    QVector<BarChart::Bar> bars;
    for (int d = 1; d <= 7; ++d) {
        m_model->appendRow({Ui::cell(ua.dayName(d), d), Ui::cell(QString::number(rows[d].count), rows[d].count),
                            Ui::cell(QString::number(rows[d].hours, 'f', 1), rows[d].hours), Ui::moneyCell(rows[d].sum)});
        bars.append({ua.standaloneDayName(d, QLocale::ShortFormat), rows[d].hours, d >= 6 ? QColor("#7b9cff") : QColor()});
    }
    m_chart->setTitle(QStringLiteral("Заброньовано годин за днями тижня"));
    m_chart->setValueSuffix(QStringLiteral("год"));
    m_chart->setBars(bars);
    m_summary->setText(QStringLiteral("Звіт допомагає планувати графік персоналу та акції на менш завантажені дні."));
}

QString ReportsPage::reportHtml() const
{
    const QString period = QStringLiteral("<p>Період: %1 – %2</p>")
                               .arg(m_from->date().toString("dd.MM.yyyy"), m_to->date().toString("dd.MM.yyyy"));
    // графік вставляємо в документ як зображення
    const QPixmap chart = m_chart->grab();
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    chart.save(&buffer, "PNG");
    const QString img = QStringLiteral("<p><img width='640' src='data:image/png;base64,%1'></p>")
                            .arg(QString::fromLatin1(png.toBase64()));
    return Exporter::wrapHtml(m_type->currentText(),
                              period + img + Exporter::modelToHtml(m_model) + "<p>" + m_summary->text() + "</p>");
}

void ReportsPage::savePdf()
{
    const QString name = QFileDialog::getSaveFileName(this, QStringLiteral("Зберегти звіт"),
                                                      QStringLiteral("report_%1.pdf").arg(QDate::currentDate().toString("yyyy-MM-dd")),
                                                      QStringLiteral("PDF (*.pdf)"));
    if (name.isEmpty())
        return;
    QString error;
    if (Exporter::savePdf(name, reportHtml(), &error))
        QMessageBox::information(this, QStringLiteral("Звіт"), QStringLiteral("Звіт збережено:\n%1").arg(name));
    else
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
}

void ReportsPage::saveCsv()
{
    const QString name = QFileDialog::getSaveFileName(this, QStringLiteral("Зберегти таблицю"),
                                                      QStringLiteral("report.csv"), QStringLiteral("CSV (*.csv)"));
    if (name.isEmpty())
        return;
    QString error;
    if (!Exporter::saveCsv(name, m_model, &error))
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
}

void ReportsPage::print()
{
    Exporter::printPreview(reportHtml(), this);
}
