#include "bookingdialog.h"
#include "clientspage.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/pricing.h"
#include "core/scheduling.h"
#include "core/session.h"
#include "theme.h"
#include "widgets.h"

#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

using Pricing::Money;

namespace {
// стовпці таблиці реквізиту
enum PropColumn { ColName, ColFree, ColPrice, ColQty };
const int PropIdRole = Qt::UserRole;
const int PriceRole = Qt::UserRole + 1;
}

BookingDialog::BookingDialog(int bookingId, QWidget *parent)
    : QDialog(parent), m_id(bookingId)
{
    setWindowTitle(m_id ? QStringLiteral("Бронювання №%1").arg(m_id) : QStringLiteral("Нове бронювання"));
    setMinimumSize(920, 620);

    // --- основні поля ---
    m_client = new QComboBox;
    m_client->setEditable(true);
    m_client->setInsertPolicy(QComboBox::NoInsert);
    m_client->completer()->setFilterMode(Qt::MatchContains);
    m_client->completer()->setCompletionMode(QCompleter::PopupCompletion);
    m_client->lineEdit()->setPlaceholderText(QStringLiteral("Почніть вводити ім'я або телефон"));
    auto *btnClient = new QPushButton(Theme::icon("plus"), QString());
    btnClient->setToolTip(QStringLiteral("Новий клієнт"));
    connect(btnClient, &QPushButton::clicked, this, &BookingDialog::addClient);
    auto *clientRow = new QHBoxLayout;
    clientRow->addWidget(m_client, 1);
    clientRow->addWidget(btnClient);

    m_hall = new QComboBox;
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat("dd.MM.yyyy");
    m_start = new QComboBox;
    m_duration = new QDoubleSpinBox;
    m_duration->setRange(1, 12);
    m_duration->setSingleStep(0.5);
    m_duration->setDecimals(1);
    m_duration->setValue(2);
    m_duration->setSuffix(QStringLiteral(" год"));

    m_status = new QComboBox;
    for (const char *code : {"new", "confirmed", "completed", "cancelled"})
        m_status->addItem(Ui::bookingStatusText(code), QString::fromLatin1(code));

    auto *timeRow = new QHBoxLayout;
    timeRow->addWidget(m_date, 1);
    timeRow->addWidget(new QLabel(QStringLiteral("з")));
    timeRow->addWidget(m_start);
    timeRow->addWidget(new QLabel(QStringLiteral("тривалість")));
    timeRow->addWidget(m_duration);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->addRow(QStringLiteral("Клієнт:"), clientRow);
    form->addRow(QStringLiteral("Зал:"), m_hall);
    form->addRow(QStringLiteral("Дата і час:"), timeRow);
    form->addRow(QStringLiteral("Статус:"), m_status);

    // --- реквізит ---
    m_props = new QTableWidget(0, 4);
    m_props->setHorizontalHeaderLabels({QStringLiteral("Реквізит"), QStringLiteral("Вільно"),
                                        QStringLiteral("Ціна за год"), QStringLiteral("Кількість")});
    m_props->verticalHeader()->hide();
    m_props->verticalHeader()->setDefaultSectionSize(34);
    m_props->setSelectionMode(QAbstractItemView::NoSelection);
    m_props->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_props->setFocusPolicy(Qt::NoFocus);
    m_props->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    auto *propsBox = new QGroupBox(QStringLiteral("Реквізит до зйомки"));
    auto *propsLay = new QVBoxLayout(propsBox);
    propsLay->addWidget(m_props);

    m_notes = new QPlainTextEdit;
    m_notes->setPlaceholderText(QStringLiteral("Побажання клієнта, кількість людей, тип зйомки..."));
    m_notes->setMaximumHeight(70);

    auto *left = new QVBoxLayout;
    left->addLayout(form);
    left->addWidget(propsBox, 1);
    left->addWidget(new QLabel(QStringLiteral("Примітки:")));
    left->addWidget(m_notes);

    // --- розрахунок ---
    m_discount = new QSpinBox;
    m_discount->setRange(0, Pricing::MaxDiscount);
    m_discount->setSuffix(" %");
    m_prepaid = new QDoubleSpinBox;
    m_prepaid->setRange(0, 1000000);
    m_prepaid->setDecimals(2);
    m_prepaid->setSuffix(QStringLiteral(" грн"));

    m_loyalty = new QLabel;
    m_loyalty->setObjectName("hint");
    m_loyalty->setWordWrap(true);
    m_hallCost = new QLabel;
    m_propsCost = new QLabel;
    m_discountSum = new QLabel;
    m_total = new QLabel;
    m_total->setObjectName("totalLabel");
    m_toPay = new QLabel;
    for (QLabel *l : {m_hallCost, m_propsCost, m_discountSum, m_total, m_toPay})
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_warning = new QLabel;
    m_warning->setObjectName("warningLabel");
    m_warning->setWordWrap(true);
    m_warning->hide();
    m_findFree = new QPushButton(Theme::icon("search"), QStringLiteral("Знайти найближчий вільний час"));
    m_findFree->hide();
    connect(m_findFree, &QPushButton::clicked, this, &BookingDialog::findFreeTime);

    auto *sumForm = new QFormLayout;
    sumForm->addRow(QStringLiteral("Оренда залу:"), m_hallCost);
    sumForm->addRow(QStringLiteral("Реквізит:"), m_propsCost);
    sumForm->addRow(QStringLiteral("Знижка:"), m_discount);
    sumForm->addRow(QString(), m_discountSum);
    sumForm->addRow(QStringLiteral("До сплати:"), m_total);
    sumForm->addRow(QStringLiteral("Передплата:"), m_prepaid);
    sumForm->addRow(QStringLiteral("Залишок:"), m_toPay);

    auto *sumBox = new QFrame;
    sumBox->setObjectName("card");
    auto *sumLay = new QVBoxLayout(sumBox);
    auto *sumTitle = new QLabel(QStringLiteral("Розрахунок"));
    sumTitle->setObjectName("cardHeader");
    sumLay->addWidget(sumTitle);
    sumLay->addLayout(sumForm);
    sumLay->addWidget(m_loyalty);
    sumLay->addStretch();
    sumLay->addWidget(m_warning);
    sumLay->addWidget(m_findFree);

    auto *right = new QVBoxLayout;
    right->addWidget(sumBox, 1);

    auto *body = new QHBoxLayout;
    body->setSpacing(18);
    body->addLayout(left, 3);
    body->addLayout(right, 2);

    auto *buttons = new QDialogButtonBox;
    m_saveBtn = buttons->addButton(QStringLiteral("Зберегти"), QDialogButtonBox::AcceptRole);
    m_saveBtn->setObjectName("primary");
    buttons->addButton(QStringLiteral("Закрити"), QDialogButtonBox::RejectRole);
    if (m_id) {
        auto *print = buttons->addButton(QStringLiteral("Квитанція"), QDialogButtonBox::ActionRole);
        print->setIcon(Theme::icon("print"));
        connect(print, &QPushButton::clicked, this, &BookingDialog::printReceipt);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &BookingDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->addLayout(body, 1);
    lay->addWidget(buttons);

    m_loading = true;
    loadReferences();
    fillTimes();
    if (m_id)
        loadBooking();
    m_loading = false;

    // будь-яка зміна полів перераховує суму та перевіряє розклад
    connect(m_client, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookingDialog::recalc);
    connect(m_hall, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookingDialog::recalc);
    connect(m_date, &QDateEdit::dateChanged, this, &BookingDialog::recalc);
    connect(m_start, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookingDialog::recalc);
    connect(m_duration, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BookingDialog::recalc);
    connect(m_status, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookingDialog::recalc);
    connect(m_discount, QOverload<int>::of(&QSpinBox::valueChanged), this, &BookingDialog::recalc);
    connect(m_prepaid, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BookingDialog::recalc);
    recalc();
}

void BookingDialog::loadReferences()
{
    QSqlQuery q("SELECT id, full_name, phone FROM clients ORDER BY full_name");
    while (q.next())
        m_client->addItem(q.value(1).toString() + "  " + q.value(2).toString(), q.value(0));
    m_client->setCurrentIndex(-1);

    q.exec("SELECT id, name, price_per_hour, color FROM halls WHERE active = 1 ORDER BY id");
    while (q.next()) {
        QPixmap dot(12, 12);
        dot.fill(QColor(q.value(3).toString()));
        m_hall->addItem(QIcon(dot), QStringLiteral("%1 — %2/год").arg(q.value(1).toString(),
                                                                       Exporter::money(q.value(2).toLongLong())),
                        q.value(0));
        m_hall->setItemData(m_hall->count() - 1, q.value(2), PriceRole);
    }

    q.exec("SELECT id, name, price_per_hour FROM props WHERE active = 1 AND price_per_hour > 0 ORDER BY category, name");
    while (q.next()) {
        const int r = m_props->rowCount();
        m_props->insertRow(r);
        auto *name = new QTableWidgetItem(q.value(1).toString());
        name->setData(PropIdRole, q.value(0));
        name->setData(PriceRole, q.value(2));
        m_props->setItem(r, ColName, name);
        m_props->setItem(r, ColFree, new QTableWidgetItem);
        m_props->setItem(r, ColPrice, new QTableWidgetItem(Exporter::money(q.value(2).toLongLong())));
        auto *spin = new QSpinBox;
        spin->setRange(0, 0);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BookingDialog::recalc);
        m_props->setCellWidget(r, ColQty, spin);
    }
    m_props->resizeColumnToContents(ColFree);
    m_props->resizeColumnToContents(ColPrice);
}

void BookingDialog::fillTimes()
{
    const QTime open = Session::instance().openTime();
    const QTime close = Session::instance().closeTime();
    for (QTime t = open; t < close && t.isValid(); t = t.addSecs(1800)) {
        m_start->addItem(t.toString("HH:mm"), t);
        if (t.addSecs(1800) < t)   // перехід через північ
            break;
    }
    const int idx = m_start->findText("10:00");
    m_start->setCurrentIndex(idx >= 0 ? idx : 0);
}

void BookingDialog::loadBooking()
{
    QSqlQuery q;
    q.prepare("SELECT client_id, hall_id, start_at, end_at, status, discount, prepaid, notes FROM bookings WHERE id = ?");
    q.addBindValue(m_id);
    if (!q.exec() || !q.next())
        return;

    m_client->setCurrentIndex(m_client->findData(q.value(0)));
    // зал міг бути виведений з використання - тоді додаємо його до списку
    int hallIdx = m_hall->findData(q.value(1));
    if (hallIdx < 0) {
        QSqlQuery h;
        h.prepare("SELECT name, price_per_hour FROM halls WHERE id = ?");
        h.addBindValue(q.value(1));
        if (h.exec() && h.next()) {
            m_hall->addItem(h.value(0).toString() + QStringLiteral(" (неактивний)"), q.value(1));
            m_hall->setItemData(m_hall->count() - 1, h.value(1), PriceRole);
            hallIdx = m_hall->count() - 1;
        }
    }
    m_hall->setCurrentIndex(hallIdx);

    const QDateTime from = Db::dtFromDb(q.value(2));
    const QDateTime to = Db::dtFromDb(q.value(3));
    m_date->setDate(from.date());
    int timeIdx = m_start->findText(from.time().toString("HH:mm"));
    if (timeIdx < 0) {
        m_start->addItem(from.time().toString("HH:mm"), from.time());
        timeIdx = m_start->count() - 1;
    }
    m_start->setCurrentIndex(timeIdx);
    m_duration->setValue(from.secsTo(to) / 3600.0);
    m_status->setCurrentIndex(m_status->findData(q.value(4)));
    m_discount->setValue(q.value(5).toInt());
    m_prepaid->setValue(Pricing::toHryvnia(q.value(6).toLongLong()));
    m_notes->setPlainText(q.value(7).toString());

    q.prepare("SELECT prop_id, qty FROM booking_props WHERE booking_id = ?");
    q.addBindValue(m_id);
    if (q.exec()) {
        while (q.next()) {
            for (int r = 0; r < m_props->rowCount(); ++r) {
                if (m_props->item(r, ColName)->data(PropIdRole) == q.value(0)) {
                    auto *spin = qobject_cast<QSpinBox *>(m_props->cellWidget(r, ColQty));
                    spin->setMaximum(q.value(1).toInt());
                    spin->setValue(q.value(1).toInt());
                }
            }
        }
    }
}

void BookingDialog::preset(int hallId, const QDateTime &start)
{
    m_loading = true;
    m_hall->setCurrentIndex(m_hall->findData(hallId));
    m_date->setDate(start.date());
    const int idx = m_start->findText(start.time().toString("HH:mm"));
    if (idx >= 0)
        m_start->setCurrentIndex(idx);
    m_loading = false;
    recalc();
}

QDateTime BookingDialog::startDateTime() const
{
    return QDateTime(m_date->date(), m_start->currentData().toTime());
}

QDateTime BookingDialog::endDateTime() const
{
    return startDateTime().addSecs(durationMinutes() * 60);
}

int BookingDialog::durationMinutes() const
{
    return qRound(m_duration->value() * 60);
}

int BookingDialog::currentClientId() const
{
    // текст має точно відповідати одному з клієнтів у списку
    const int idx = m_client->findText(m_client->currentText());
    return idx >= 0 ? m_client->itemData(idx).toInt() : 0;
}

void BookingDialog::recalc()
{
    if (m_loading)
        return;

    const QDateTime from = startDateTime();
    const QDateTime to = endDateTime();
    const int halves = Pricing::halfHours(from, to);
    const QString status = m_status->currentData().toString();
    QStringList problems;

    // оренда залу
    const Money hallPrice = m_hall->currentData(PriceRole).toLongLong();
    const Money hallCost = Pricing::hallCost(hallPrice, from, to);
    m_hallCost->setText(Exporter::money(hallCost)
                        + (Pricing::isWeekend(from.date()) ? QStringLiteral("  (вихідний +20 %)") : QString()));

    // реквізит: доступна кількість на обраний проміжок
    Money propsCost = 0;
    for (int r = 0; r < m_props->rowCount(); ++r) {
        const int propId = m_props->item(r, ColName)->data(PropIdRole).toInt();
        const int free = Db::propAvailable(propId, from, to, m_id);
        auto *spin = qobject_cast<QSpinBox *>(m_props->cellWidget(r, ColQty));
        m_props->item(r, ColFree)->setText(QString::number(free));
        m_props->item(r, ColFree)->setForeground(free == 0 ? Theme::Danger : QColor("#1f2433"));
        spin->blockSignals(true);
        spin->setMaximum(qMax(free, spin->value()));
        spin->blockSignals(false);
        if (spin->value() > free && status != "cancelled")
            problems << QStringLiteral("«%1» доступно лише %2 шт.").arg(m_props->item(r, ColName)->text()).arg(free);
        propsCost += Pricing::propCostHourly(m_props->item(r, ColName)->data(PriceRole).toLongLong(),
                                             spin->value(), halves);
    }
    m_propsCost->setText(Exporter::money(propsCost));

    // знижка: постійним клієнтам нараховується автоматично
    const int clientId = currentClientId();
    const int visits = clientId ? Db::completedVisits(clientId) : 0;
    const int loyalty = Pricing::loyaltyDiscount(visits);
    m_loyalty->setText(clientId ? QStringLiteral("Завершених візитів клієнта: %1. Накопичувальна знижка: %2 %.")
                                      .arg(visits).arg(loyalty)
                                : QString());
    const int discount = Pricing::effectiveDiscount(m_discount->value(), loyalty);
    const Money subtotal = hallCost + propsCost;
    m_totalValue = Pricing::applyDiscount(subtotal, discount);
    m_discountSum->setText(discount ? QStringLiteral("−%1 (діє %2 %)").arg(Exporter::money(subtotal - m_totalValue)).arg(discount)
                                    : QStringLiteral("—"));
    m_total->setText(Exporter::money(m_totalValue));
    const Money prepaid = Pricing::fromHryvnia(m_prepaid->value());
    m_toPay->setText(Exporter::money(qMax<Money>(0, m_totalValue - prepaid)));

    // перевірки розкладу
    m_findFree->hide();
    if (!clientId)
        problems << QStringLiteral("Оберіть клієнта зі списку або додайте нового.");
    if (m_hall->currentIndex() < 0)
        problems << QStringLiteral("Оберіть зал.");
    if (!Scheduling::withinWorkingHours(from, to, Session::instance().openTime(), Session::instance().closeTime()))
        problems << QStringLiteral("Бронювання виходить за межі робочого часу (%1–%2).")
                        .arg(Session::instance().openTime().toString("HH:mm"),
                             Session::instance().closeTime().toString("HH:mm"));
    if (m_hall->currentIndex() >= 0 && status != "cancelled") {
        const int conflict = Db::hallConflict(m_hall->currentData().toInt(), from, to, m_id);
        if (conflict) {
            const QString when = Db::scalar("SELECT substr(start_at,12,5) || '–' || substr(end_at,12,5)"
                                            " FROM bookings WHERE id = ?", {conflict}).toString();
            problems << QStringLiteral("Зал уже зайнятий: бронювання №%1 (%2).").arg(conflict).arg(when);
            m_findFree->show();
        }
    }
    if (prepaid > m_totalValue)
        problems << QStringLiteral("Передплата більша за суму до сплати.");

    m_valid = problems.isEmpty();
    m_warning->setText(problems.join('\n'));
    m_warning->setVisible(!m_valid);
    m_saveBtn->setEnabled(m_valid);
}

void BookingDialog::findFreeTime()
{
    const int hallId = m_hall->currentData().toInt();
    const QDate date = m_date->date();
    const Session &s = Session::instance();

    // шукаємо в обраний день, а якщо місць немає - у наступні 7 днів
    for (int d = 0; d < 7; ++d) {
        const QDate day = date.addDays(d);
        const QDateTime slot = Scheduling::findFreeSlot(Db::hallBusy(hallId, day, m_id), day, durationMinutes(),
                                                        s.openTime(), s.closeTime(), m_start->currentData().toTime());
        if (slot.isValid()) {
            m_date->setDate(day);
            const int idx = m_start->findText(slot.time().toString("HH:mm"));
            if (idx >= 0)
                m_start->setCurrentIndex(idx);
            return;
        }
    }
    QMessageBox::information(this, QStringLiteral("Пошук часу"),
                             QStringLiteral("Протягом тижня немає вільного часу такої тривалості в цьому залі."));
}

void BookingDialog::addClient()
{
    const int id = ClientDialog::createClient(this);
    if (!id)
        return;
    QSqlQuery q;
    q.prepare("SELECT full_name, phone FROM clients WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        m_client->addItem(q.value(0).toString() + "  " + q.value(1).toString(), id);
        m_client->setCurrentIndex(m_client->count() - 1);
    }
}

void BookingDialog::save()
{
    recalc();
    if (!m_valid)
        return;

    const QDateTime from = startDateTime();
    const QDateTime to = endDateTime();
    const int halves = Pricing::halfHours(from, to);
    const Money hallCost = Pricing::hallCost(m_hall->currentData(PriceRole).toLongLong(), from, to);
    const int discount = Pricing::effectiveDiscount(m_discount->value(),
                                                    Pricing::loyaltyDiscount(Db::completedVisits(currentClientId())));

    struct Item { int propId; int qty; Money price; };
    QList<Item> items;
    Money propsCost = 0;
    for (int r = 0; r < m_props->rowCount(); ++r) {
        const int qty = qobject_cast<QSpinBox *>(m_props->cellWidget(r, ColQty))->value();
        if (qty <= 0)
            continue;
        const Money price = m_props->item(r, ColName)->data(PriceRole).toLongLong();
        items.append({m_props->item(r, ColName)->data(PropIdRole).toInt(), qty, price});
        propsCost += Pricing::propCostHourly(price, qty, halves);
    }
    const Money total = Pricing::applyDiscount(hallCost + propsCost, discount);

    QVariantList values = {currentClientId(), m_hall->currentData(), Db::toDb(from), Db::toDb(to),
                           m_status->currentData(), hallCost, propsCost, discount, total,
                           Pricing::fromHryvnia(m_prepaid->value()), m_notes->toPlainText().trimmed()};

    // бронювання і список реквізиту зберігаються разом в одній транзакції
    const int originalId = m_id;
    QString error;
    const bool ok = Db::transaction([&](QString *err) {
        // повторна перевірка безпосередньо перед записом
        if (m_status->currentData() != "cancelled" && Db::hallConflict(m_hall->currentData().toInt(), from, to, m_id)) {
            *err = QStringLiteral("Поки форма була відкрита, цей час уже забронювали.");
            return false;
        }
        if (m_id == 0) {
            values << Session::instance().userId << Db::toDb(QDateTime::currentDateTime());
            if (!Db::exec("INSERT INTO bookings (client_id, hall_id, start_at, end_at, status, hall_cost, props_cost,"
                          " discount, total, prepaid, notes, created_by, created_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                          values, err))
                return false;
            m_id = Db::scalar("SELECT last_insert_rowid()").toInt();
        } else {
            values << m_id;
            int rows = 0;
            if (!Db::exec("UPDATE bookings SET client_id = ?, hall_id = ?, start_at = ?, end_at = ?, status = ?,"
                          " hall_cost = ?, props_cost = ?, discount = ?, total = ?, prepaid = ?, notes = ?"
                          " WHERE id = ?", values, err, &rows))
                return false;
            if (rows != 1) {
                *err = QStringLiteral("Бронювання №%1 не знайдено (можливо, його видалено).").arg(m_id);
                return false;
            }
            if (!Db::exec("DELETE FROM booking_props WHERE booking_id = ?", {m_id}, err))
                return false;
        }
        for (const Item &i : items) {
            if (!Db::exec("INSERT INTO booking_props (booking_id, prop_id, qty, price) VALUES (?, ?, ?, ?)",
                          {m_id, i.propId, i.qty, i.price}, err))
                return false;
        }
        return true;
    }, &error);

    if (!ok) {
        m_id = originalId;   // після відкату новий запис не існує
        QMessageBox::warning(this, QStringLiteral("Бронювання не збережено"), error);
        return;
    }
    Db::log(QStringLiteral("Збережено бронювання"), QStringLiteral("№%1").arg(m_id));
    accept();
}

void BookingDialog::printReceipt()
{
    Exporter::printPreview(receiptHtml(m_id), this);
}

QString BookingDialog::receiptHtml(int bookingId)
{
    QSqlQuery q;
    q.prepare("SELECT b.start_at, b.end_at, b.status, b.hall_cost, b.props_cost, b.discount, b.total, b.prepaid,"
              " c.full_name, c.phone, h.name, h.price_per_hour, b.notes FROM bookings b"
              " JOIN clients c ON c.id = b.client_id JOIN halls h ON h.id = b.hall_id WHERE b.id = ?");
    q.addBindValue(bookingId);
    if (!q.exec() || !q.next())
        return QString();

    const QDateTime from = Db::dtFromDb(q.value(0));
    const QDateTime to = Db::dtFromDb(q.value(1));
    const int halves = Pricing::halfHours(from, to);
    QString rows = QStringLiteral("<tr><td>Оренда залу «%1», %2 год%3</td><td align='right'>%4</td></tr>")
                       .arg(q.value(10).toString().toHtmlEscaped())
                       .arg(halves / 2.0, 0, 'f', 1)
                       .arg(Pricing::isWeekend(from.date()) ? QStringLiteral(" (вихідний +20 %)") : QString())
                       .arg(Exporter::money(q.value(3).toLongLong()));

    QSqlQuery p;
    p.prepare("SELECT pr.name, bp.qty, bp.price FROM booking_props bp JOIN props pr ON pr.id = bp.prop_id"
              " WHERE bp.booking_id = ?");
    p.addBindValue(bookingId);
    if (p.exec()) {
        while (p.next()) {
            rows += QStringLiteral("<tr><td>%1 × %2 шт</td><td align='right'>%3</td></tr>")
                        .arg(p.value(0).toString().toHtmlEscaped())
                        .arg(p.value(1).toInt())
                        .arg(Exporter::money(Pricing::propCostHourly(p.value(2).toLongLong(), p.value(1).toInt(), halves)));
        }
    }

    const Money total = q.value(6).toLongLong();
    const Money prepaid = q.value(7).toLongLong();
    QString body = QStringLiteral(
        "<p>Клієнт: <b>%1</b>, %2<br>Дата: <b>%3</b>, %4–%5<br>Статус: %6</p>"
        "<table class='grid' width='100%' cellspacing='0' cellpadding='5'>"
        "<tr><th>Послуга</th><th align='right'>Сума</th></tr>%7")
        .arg(q.value(8).toString().toHtmlEscaped(), q.value(9).toString(), from.toString("dd.MM.yyyy"),
             from.toString("HH:mm"), to.toString("HH:mm"), Ui::bookingStatusText(q.value(2).toString()), rows);
    if (q.value(5).toInt() > 0)
        body += QStringLiteral("<tr><td>Знижка %1 %</td><td align='right'>−%2</td></tr>")
                    .arg(q.value(5).toInt())
                    .arg(Exporter::money(q.value(3).toLongLong() + q.value(4).toLongLong() - total));
    body += QStringLiteral("</table><p class='total'>Разом: %1</p><p>Сплачено: %2<br>Залишок до сплати: %3</p>")
                .arg(Exporter::money(total), Exporter::money(prepaid), Exporter::money(qMax<Money>(0, total - prepaid)));
    if (!q.value(12).toString().isEmpty())
        body += QStringLiteral("<p class='muted'>Примітки: %1</p>").arg(q.value(12).toString().toHtmlEscaped());
    body += QStringLiteral("<p><br>Менеджер: ____________ %1</p>").arg(Session::instance().fullName.toHtmlEscaped());

    return Exporter::wrapHtml(QStringLiteral("Квитанція до бронювання №%1").arg(bookingId), body);
}
