#include "rentaldialog.h"
#include "clientspage.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/pricing.h"
#include "core/session.h"
#include "theme.h"
#include "widgets.h"

#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
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
enum Column { ColName, ColFree, ColPrice, ColDeposit, ColQty };
const int PropIdRole = Qt::UserRole;
const int PriceRole = Qt::UserRole + 1;
const int DepositRole = Qt::UserRole + 2;
}

RentalDialog::RentalDialog(int rentalId, QWidget *parent)
    : QDialog(parent), m_id(rentalId)
{
    setWindowTitle(m_id ? QStringLiteral("Оренда №%1").arg(m_id) : QStringLiteral("Нова оренда реквізиту"));
    setMinimumSize(820, 580);

    m_client = new QComboBox;
    m_client->setEditable(true);
    m_client->setInsertPolicy(QComboBox::NoInsert);
    m_client->completer()->setFilterMode(Qt::MatchContains);
    m_client->completer()->setCompletionMode(QCompleter::PopupCompletion);
    m_client->lineEdit()->setPlaceholderText(QStringLiteral("Почніть вводити ім'я або телефон"));
    auto *btnClient = new QPushButton(Theme::icon("plus"), QString());
    btnClient->setToolTip(QStringLiteral("Новий клієнт"));
    connect(btnClient, &QPushButton::clicked, this, &RentalDialog::addClient);
    auto *clientRow = new QHBoxLayout;
    clientRow->addWidget(m_client, 1);
    clientRow->addWidget(btnClient);

    m_start = new QDateEdit(QDate::currentDate());
    m_end = new QDateEdit(QDate::currentDate().addDays(1));
    for (QDateEdit *d : {m_start, m_end}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd.MM.yyyy");
    }
    auto *dates = new QHBoxLayout;
    dates->addWidget(m_start);
    dates->addWidget(new QLabel(QStringLiteral("по")));
    dates->addWidget(m_end);
    dates->addStretch();

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Клієнт:"), clientRow);
    form->addRow(QStringLiteral("Період оренди:"), dates);

    m_items = new QTableWidget(0, 5);
    m_items->setHorizontalHeaderLabels({QStringLiteral("Реквізит"), QStringLiteral("Вільно"),
                                        QStringLiteral("Ціна за добу"), QStringLiteral("Застава"),
                                        QStringLiteral("Кількість")});
    m_items->verticalHeader()->hide();
    m_items->verticalHeader()->setDefaultSectionSize(34);
    m_items->setSelectionMode(QAbstractItemView::NoSelection);
    m_items->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_items->setFocusPolicy(Qt::NoFocus);
    m_items->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);

    m_notes = new QPlainTextEdit;
    m_notes->setMaximumHeight(60);
    m_notes->setPlaceholderText(QStringLiteral("Документ, залишений під заставу, стан реквізиту..."));

    m_days = new QLabel;
    m_total = new QLabel;
    m_total->setObjectName("totalLabel");
    m_deposit = new QLabel;
    m_warning = new QLabel;
    m_warning->setObjectName("warningLabel");
    m_warning->setWordWrap(true);
    m_warning->hide();

    auto *sum = new QHBoxLayout;
    sum->addWidget(m_days);
    sum->addStretch();
    sum->addWidget(new QLabel(QStringLiteral("Застава:")));
    sum->addWidget(m_deposit);
    sum->addSpacing(16);
    sum->addWidget(new QLabel(QStringLiteral("До сплати:")));
    sum->addWidget(m_total);

    auto *buttons = new QDialogButtonBox;
    m_saveBtn = buttons->addButton(QStringLiteral("Зберегти"), QDialogButtonBox::AcceptRole);
    m_saveBtn->setObjectName("primary");
    buttons->addButton(QStringLiteral("Закрити"), QDialogButtonBox::RejectRole);
    if (m_id) {
        auto *print = buttons->addButton(QStringLiteral("Акт видачі"), QDialogButtonBox::ActionRole);
        print->setIcon(Theme::icon("print"));
        connect(print, &QPushButton::clicked, this, [this] { Exporter::printPreview(actHtml(m_id), this); });
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &RentalDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->addLayout(form);
    lay->addWidget(m_items, 1);
    lay->addWidget(new QLabel(QStringLiteral("Примітки:")));
    lay->addWidget(m_notes);
    lay->addLayout(sum);
    lay->addWidget(m_warning);
    lay->addWidget(buttons);

    m_loading = true;
    loadReferences();
    if (m_id)
        loadRental();
    m_loading = false;

    connect(m_client, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RentalDialog::recalc);
    connect(m_client->lineEdit(), &QLineEdit::textEdited, this, &RentalDialog::recalc);
    connect(m_start, &QDateEdit::dateChanged, this, &RentalDialog::recalc);
    connect(m_end, &QDateEdit::dateChanged, this, &RentalDialog::recalc);
    recalc();

    // повернену або скасовану оренду лише переглядаємо
    if (m_readOnly) {
        for (QWidget *w : {static_cast<QWidget *>(m_client), static_cast<QWidget *>(m_start),
                           static_cast<QWidget *>(m_end), static_cast<QWidget *>(m_items),
                           static_cast<QWidget *>(btnClient)})
            w->setEnabled(false);
        m_notes->setReadOnly(true);
        m_saveBtn->hide();
    }
}

void RentalDialog::loadReferences()
{
    QSqlQuery q("SELECT id, full_name, phone FROM clients ORDER BY full_name");
    while (q.next())
        m_client->addItem(q.value(1).toString() + "  " + q.value(2).toString(), q.value(0));
    m_client->setCurrentIndex(-1);

    q.exec("SELECT id, name, price_per_day, deposit FROM props WHERE active = 1 AND price_per_day > 0"
           " ORDER BY category, name");
    while (q.next()) {
        const int r = m_items->rowCount();
        m_items->insertRow(r);
        auto *name = new QTableWidgetItem(q.value(1).toString());
        name->setData(PropIdRole, q.value(0));
        name->setData(PriceRole, q.value(2));
        name->setData(DepositRole, q.value(3));
        m_items->setItem(r, ColName, name);
        m_items->setItem(r, ColFree, new QTableWidgetItem);
        m_items->setItem(r, ColPrice, new QTableWidgetItem(Exporter::money(q.value(2).toLongLong())));
        m_items->setItem(r, ColDeposit, new QTableWidgetItem(Exporter::money(q.value(3).toLongLong())));
        auto *spin = new QSpinBox;
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &RentalDialog::recalc);
        m_items->setCellWidget(r, ColQty, spin);
    }
    m_items->resizeColumnToContents(ColFree);
    m_items->resizeColumnToContents(ColPrice);
    m_items->resizeColumnToContents(ColDeposit);
}

void RentalDialog::loadRental()
{
    QSqlQuery q;
    q.prepare("SELECT client_id, start_date, end_date, status, notes FROM rentals WHERE id = ?");
    q.addBindValue(m_id);
    if (!q.exec() || !q.next())
        return;
    m_client->setCurrentIndex(m_client->findData(q.value(0)));
    m_start->setDate(Db::dateFromDb(q.value(1)));
    m_end->setDate(Db::dateFromDb(q.value(2)));
    m_readOnly = q.value(3).toString() != "active";
    m_notes->setPlainText(q.value(4).toString());

    q.prepare("SELECT prop_id, qty FROM rental_items WHERE rental_id = ?");
    q.addBindValue(m_id);
    if (q.exec()) {
        while (q.next()) {
            for (int r = 0; r < m_items->rowCount(); ++r) {
                if (m_items->item(r, ColName)->data(PropIdRole) == q.value(0)) {
                    auto *spin = qobject_cast<QSpinBox *>(m_items->cellWidget(r, ColQty));
                    spin->setMaximum(q.value(1).toInt());
                    spin->setValue(q.value(1).toInt());
                }
            }
        }
    }
}

int RentalDialog::currentClientId() const
{
    const int idx = m_client->findText(m_client->currentText());
    return idx >= 0 ? m_client->itemData(idx).toInt() : 0;
}

void RentalDialog::recalc()
{
    if (m_loading)
        return;

    const QDate start = m_start->date();
    const QDate end = m_end->date();
    const int days = Pricing::rentalDays(start, end);
    const QDateTime from(start, QTime(0, 0));
    const QDateTime to(end.addDays(1), QTime(0, 0));
    QStringList problems;

    Money total = 0;
    Money deposit = 0;
    int units = 0;
    for (int r = 0; r < m_items->rowCount(); ++r) {
        auto *spin = qobject_cast<QSpinBox *>(m_items->cellWidget(r, ColQty));
        const int free = days > 0 ? Db::propAvailable(m_items->item(r, ColName)->data(PropIdRole).toInt(),
                                                      from, to, 0, m_id) : 0;
        m_items->item(r, ColFree)->setText(QString::number(free));
        m_items->item(r, ColFree)->setForeground(free == 0 ? Theme::Danger : QColor("#1f2433"));
        spin->blockSignals(true);
        spin->setMaximum(qMax(free, spin->value()));
        spin->blockSignals(false);
        const int qty = spin->value();
        if (qty > free)
            problems << QStringLiteral("«%1» на ці дати доступно лише %2 шт.").arg(m_items->item(r, ColName)->text()).arg(free);
        units += qty;
        total += Pricing::propCostDaily(m_items->item(r, ColName)->data(PriceRole).toLongLong(), qty, days);
        deposit += m_items->item(r, ColName)->data(DepositRole).toLongLong() * qty;
    }

    if (!currentClientId())
        problems << QStringLiteral("Оберіть клієнта зі списку або додайте нового.");
    if (days <= 0)
        problems << QStringLiteral("Дата повернення не може бути раніше дати видачі.");
    else if (days > 60)
        problems << QStringLiteral("Максимальний термін оренди - 60 днів.");
    if (units == 0)
        problems << QStringLiteral("Вкажіть кількість хоча б для одного найменування.");
    if (!m_id && start < QDate::currentDate())
        problems << QStringLiteral("Нову оренду не можна оформити заднім числом.");

    m_days->setText(QStringLiteral("Діб: %1, одиниць: %2").arg(qMax(0, days)).arg(units));
    m_total->setText(Exporter::money(total));
    m_deposit->setText(Exporter::money(deposit));

    m_valid = problems.isEmpty();
    m_warning->setText(problems.join('\n'));
    m_warning->setVisible(!m_valid);
    m_saveBtn->setEnabled(m_valid);
}

void RentalDialog::addClient()
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

void RentalDialog::save()
{
    recalc();
    if (!m_valid)
        return;

    const int days = Pricing::rentalDays(m_start->date(), m_end->date());
    struct Item { int propId; int qty; Money price; };
    QList<Item> items;
    Money total = 0;
    Money deposit = 0;
    for (int r = 0; r < m_items->rowCount(); ++r) {
        const int qty = qobject_cast<QSpinBox *>(m_items->cellWidget(r, ColQty))->value();
        if (qty <= 0)
            continue;
        const QTableWidgetItem *name = m_items->item(r, ColName);
        const Money price = name->data(PriceRole).toLongLong();
        items.append({name->data(PropIdRole).toInt(), qty, price});
        total += Pricing::propCostDaily(price, qty, days);
        deposit += name->data(DepositRole).toLongLong() * qty;
    }

    const int originalId = m_id;
    QString error;
    const bool ok = Db::transaction([&](QString *err) {
        const QVariantList common = {currentClientId(), Db::toDb(m_start->date()), Db::toDb(m_end->date()),
                                     total, deposit, m_notes->toPlainText().trimmed()};
        if (m_id == 0) {
            QVariantList args = common;
            args << Session::instance().userId << Db::toDb(QDateTime::currentDateTime());
            if (!Db::exec("INSERT INTO rentals (client_id, start_date, end_date, total, deposit, notes, created_by,"
                          " created_at) VALUES (?,?,?,?,?,?,?,?)", args, err))
                return false;
            m_id = Db::scalar("SELECT last_insert_rowid()").toInt();
        } else {
            QVariantList args = common;
            args << m_id;
            int rows = 0;
            if (!Db::exec("UPDATE rentals SET client_id = ?, start_date = ?, end_date = ?, total = ?, deposit = ?,"
                          " notes = ? WHERE id = ? AND status = 'active'", args, err, &rows))
                return false;
            if (rows != 1) {
                *err = QStringLiteral("Оренду не знайдено або її вже закрито.");
                return false;
            }
            if (!Db::exec("DELETE FROM rental_items WHERE rental_id = ?", {m_id}, err))
                return false;
        }
        for (const Item &i : items) {
            // повторна перевірка залишку всередині транзакції
            const int free = Db::propAvailable(i.propId, QDateTime(m_start->date(), QTime(0, 0)),
                                               QDateTime(m_end->date().addDays(1), QTime(0, 0)), 0, m_id);
            if (i.qty > free) {
                *err = QStringLiteral("Недостатньо реквізиту на складі (id %1).").arg(i.propId);
                return false;
            }
            if (!Db::exec("INSERT INTO rental_items (rental_id, prop_id, qty, price_per_day) VALUES (?, ?, ?, ?)",
                          {m_id, i.propId, i.qty, i.price}, err))
                return false;
        }
        return true;
    }, &error);

    if (!ok) {
        m_id = originalId;
        QMessageBox::warning(this, QStringLiteral("Оренду не збережено"), error);
        return;
    }
    Db::log(QStringLiteral("Збережено оренду"), QStringLiteral("№%1").arg(m_id));
    accept();
}

QString RentalDialog::actHtml(int rentalId)
{
    QSqlQuery q;
    q.prepare("SELECT r.start_date, r.end_date, r.total, r.deposit, r.fine, r.status, r.returned_at,"
              " c.full_name, c.phone, r.notes FROM rentals r JOIN clients c ON c.id = r.client_id WHERE r.id = ?");
    q.addBindValue(rentalId);
    if (!q.exec() || !q.next())
        return QString();
    const QDate start = Db::dateFromDb(q.value(0));
    const QDate end = Db::dateFromDb(q.value(1));
    const int days = Pricing::rentalDays(start, end);

    QString rows;
    QSqlQuery i;
    i.prepare("SELECT p.name, ri.qty, ri.price_per_day FROM rental_items ri JOIN props p ON p.id = ri.prop_id"
              " WHERE ri.rental_id = ?");
    i.addBindValue(rentalId);
    if (i.exec()) {
        int n = 1;
        while (i.next()) {
            rows += QStringLiteral("<tr><td>%1</td><td>%2</td><td align='center'>%3</td><td align='right'>%4</td>"
                                   "<td align='right'>%5</td></tr>")
                        .arg(n++).arg(i.value(0).toString().toHtmlEscaped()).arg(i.value(1).toInt())
                        .arg(Exporter::money(i.value(2).toLongLong()))
                        .arg(Exporter::money(Pricing::propCostDaily(i.value(2).toLongLong(), i.value(1).toInt(), days)));
        }
    }

    QString body = QStringLiteral(
        "<p>Орендар: <b>%1</b>, %2<br>Період: з %3 по %4 (%5 діб)</p>"
        "<table class='grid' width='100%' cellspacing='0' cellpadding='5'>"
        "<tr><th>№</th><th>Найменування</th><th>К-сть</th><th>Ціна за добу</th><th>Сума</th></tr>%6</table>"
        "<p class='total'>Вартість оренди: %7</p><p>Застава: %8</p>")
        .arg(q.value(7).toString().toHtmlEscaped(), q.value(8).toString(), start.toString("dd.MM.yyyy"),
             end.toString("dd.MM.yyyy"))
        .arg(days)
        .arg(rows, Exporter::money(q.value(2).toLongLong()), Exporter::money(q.value(3).toLongLong()));
    if (q.value(5).toString() == "returned") {
        body += QStringLiteral("<p>Повернуто: %1").arg(Db::dtFromDb(q.value(6)).toString("dd.MM.yyyy HH:mm"));
        if (q.value(4).toLongLong() > 0)
            body += QStringLiteral(", штраф за прострочення: %1").arg(Exporter::money(q.value(4).toLongLong()));
        body += "</p>";
    }
    body += QStringLiteral("<p>Реквізит отримано у справному стані. З умовами оренди ознайомлений(-а).</p>"
                           "<p><br>Орендар: ____________ &nbsp;&nbsp;&nbsp; Менеджер: ____________ %1</p>")
                .arg(Session::instance().fullName.toHtmlEscaped());
    return Exporter::wrapHtml(QStringLiteral("Акт видачі реквізиту №%1").arg(rentalId), body);
}
