#include "clientspage.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/pricing.h"
#include "theme.h"
#include "widgets.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSplitter>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

// ---------------- ClientDialog ----------------

ClientDialog::ClientDialog(int clientId, QWidget *parent)
    : QDialog(parent), m_id(clientId)
{
    setWindowTitle(m_id ? QStringLiteral("Редагування клієнта") : QStringLiteral("Новий клієнт"));
    setMinimumWidth(440);

    m_name = new QLineEdit;
    m_name->setMaxLength(100);
    m_name->setPlaceholderText(QStringLiteral("Прізвище та ім'я"));
    m_phone = new QLineEdit;
    m_phone->setPlaceholderText("+380XXXXXXXXX");
    m_phone->setMaxLength(19);
    m_email = new QLineEdit;
    m_email->setMaxLength(100);
    m_email->setPlaceholderText("name@example.com");
    m_notes = new QPlainTextEdit;
    m_notes->setMaximumHeight(80);
    m_error = new QLabel;
    m_error->setObjectName("warningLabel");
    m_error->setWordWrap(true);
    m_error->hide();

    if (m_id) {
        QSqlQuery q;
        q.prepare("SELECT full_name, phone, email, notes FROM clients WHERE id = ?");
        q.addBindValue(m_id);
        if (q.exec() && q.next()) {
            m_name->setText(q.value(0).toString());
            m_phone->setText(q.value(1).toString());
            m_email->setText(q.value(2).toString());
            m_notes->setPlainText(q.value(3).toString());
        }
    }

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("ПІБ*:"), m_name);
    form->addRow(QStringLiteral("Телефон*:"), m_phone);
    form->addRow(QStringLiteral("E-mail:"), m_email);
    form->addRow(QStringLiteral("Примітки:"), m_notes);

    auto *buttons = new QDialogButtonBox;
    QPushButton *ok = buttons->addButton(QStringLiteral("Зберегти"), QDialogButtonBox::AcceptRole);
    ok->setObjectName("primary");
    buttons->addButton(QStringLiteral("Скасувати"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &ClientDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(m_error);
    lay->addWidget(buttons);
}

void ClientDialog::save()
{
    const QString name = m_name->text().simplified();
    const QString phone = Db::normalizePhone(m_phone->text());
    const QString email = m_email->text().trimmed();

    QStringList problems;
    if (name.length() < 2)
        problems << QStringLiteral("Вкажіть ПІБ клієнта (щонайменше 2 символи).");
    if (phone.isEmpty())
        problems << QStringLiteral("Номер телефону має бути український: +380XXXXXXXXX або 0XXXXXXXXX.");
    static const QRegularExpression emailRe("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$");
    if (!email.isEmpty() && !emailRe.match(email).hasMatch())
        problems << QStringLiteral("Некоректна адреса e-mail.");
    if (!phone.isEmpty()
        && Db::scalar("SELECT COUNT(*) FROM clients WHERE phone = ? AND id <> ?", {phone, m_id}).toInt() > 0)
        problems << QStringLiteral("Клієнт з номером %1 уже є в базі.").arg(phone);

    if (!problems.isEmpty()) {
        m_error->setText(problems.join('\n'));
        m_error->show();
        return;
    }

    QString error;
    bool ok;
    if (m_id) {
        int rows = 0;
        ok = Db::exec("UPDATE clients SET full_name = ?, phone = ?, email = ?, notes = ? WHERE id = ?",
                      {name, phone, email, m_notes->toPlainText().trimmed(), m_id}, &error, &rows) && rows == 1;
    } else {
        ok = Db::exec("INSERT INTO clients (full_name, phone, email, notes, created_at) VALUES (?, ?, ?, ?, ?)",
                      {name, phone, email, m_notes->toPlainText().trimmed(), Db::toDb(QDateTime::currentDateTime())},
                      &error);
        if (ok)
            m_id = Db::scalar("SELECT last_insert_rowid()").toInt();
    }
    if (!ok) {
        m_error->setText(QStringLiteral("Не вдалося зберегти: %1").arg(error));
        m_error->show();
        return;
    }
    accept();
}

int ClientDialog::createClient(QWidget *parent)
{
    ClientDialog dlg(0, parent);
    return dlg.exec() == QDialog::Accepted ? dlg.clientId() : 0;
}

// ---------------- ClientsPage ----------------

ClientsPage::ClientsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *btnAdd = new QPushButton(Theme::icon("plus", Qt::white), QStringLiteral("Новий клієнт"));
    btnAdd->setObjectName("primary");
    connect(btnAdd, &QPushButton::clicked, this, &ClientsPage::addClient);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Пошук за ім'ям, телефоном або e-mail"));
    m_search->addAction(Theme::icon("search"), QLineEdit::LeadingPosition);
    m_search->setClearButtonEnabled(true);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("ПІБ"), QStringLiteral("Телефон"), QStringLiteral("E-mail"),
                                        QStringLiteral("Візитів"), QStringLiteral("Сума покупок"),
                                        QStringLiteral("Знижка")});
    m_proxy = new TableProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterKeyColumn(-1);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    connect(m_search, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);

    m_view = new QTableView;
    m_view->setModel(m_proxy);
    Ui::setupTable(m_view);
    connect(m_view, &QTableView::doubleClicked, this, &ClientsPage::editSelected);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ClientsPage::showHistory);

    m_historyTitle = new QLabel(QStringLiteral("Історія клієнта"));
    m_historyTitle->setObjectName("cardHeader");
    m_history = new QTableWidget(0, 4);
    m_history->setHorizontalHeaderLabels({QStringLiteral("Дата"), QStringLiteral("Послуга"),
                                          QStringLiteral("Сума"), QStringLiteral("Статус")});
    m_history->verticalHeader()->hide();
    m_history->verticalHeader()->setDefaultSectionSize(32);
    m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_history->setSelectionMode(QAbstractItemView::NoSelection);
    m_history->setShowGrid(false);
    m_history->horizontalHeader()->setStretchLastSection(true);
    m_history->setItemDelegateForColumn(3, new StatusDelegate(m_history));

    auto *historyCard = new QFrame;
    historyCard->setObjectName("card");
    auto *hl = new QVBoxLayout(historyCard);
    hl->addWidget(m_historyTitle);
    hl->addWidget(m_history, 1);

    auto *split = new QSplitter;
    split->addWidget(m_view);
    split->addWidget(historyCard);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);

    auto *btnEdit = new QPushButton(Theme::icon("edit"), QStringLiteral("Редагувати"));
    auto *btnDelete = new QPushButton(Theme::icon("trash"), QStringLiteral("Видалити"));
    btnDelete->setObjectName("danger");
    auto *btnImport = new QPushButton(Theme::icon("upload"), QStringLiteral("Імпорт CSV"));
    auto *btnExport = new QPushButton(Theme::icon("download"), QStringLiteral("Експорт CSV"));
    connect(btnEdit, &QPushButton::clicked, this, &ClientsPage::editSelected);
    connect(btnDelete, &QPushButton::clicked, this, &ClientsPage::deleteSelected);
    connect(btnImport, &QPushButton::clicked, this, &ClientsPage::importCsv);
    connect(btnExport, &QPushButton::clicked, this, &ClientsPage::exportCsv);

    auto *actions = new QHBoxLayout;
    actions->addWidget(btnEdit);
    actions->addWidget(btnDelete);
    actions->addStretch();
    actions->addWidget(btnImport);
    actions->addWidget(btnExport);

    m_footer = new QLabel;
    m_footer->setObjectName("pageSubtitle");

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);
    lay->addWidget(Ui::pageHeader(QStringLiteral("Клієнти"),
                                  QStringLiteral("База клієнтів, історія візитів і накопичувальні знижки"), btnAdd));
    lay->addWidget(m_search);
    lay->addWidget(split, 1);
    lay->addLayout(actions);
    lay->addWidget(m_footer);
}

void ClientsPage::refresh()
{
    const int keep = selectedId();
    m_model->removeRows(0, m_model->rowCount());

    // кількість візитів і сума покупок рахуються підзапитами з агрегуванням
    QSqlQuery q("SELECT c.id, c.full_name, c.phone, c.email,"
                " (SELECT COUNT(*) FROM bookings b WHERE b.client_id = c.id AND b.status = 'completed'),"
                " (SELECT COALESCE(SUM(total),0) FROM bookings b WHERE b.client_id = c.id AND b.status = 'completed')"
                " + (SELECT COALESCE(SUM(total + fine),0) FROM rentals r WHERE r.client_id = c.id AND r.status = 'returned')"
                " FROM clients c ORDER BY c.full_name");
    int row = -1;
    while (q.next()) {
        const int visits = q.value(4).toInt();
        const int discount = Pricing::loyaltyDiscount(visits);
        QStandardItem *name = Ui::cell(q.value(1).toString());
        name->setData(q.value(0), Ui::IdRole);
        m_model->appendRow({name, Ui::cell(q.value(2).toString()), Ui::cell(q.value(3).toString()),
                            Ui::cell(QString::number(visits), visits), Ui::moneyCell(q.value(5).toLongLong()),
                            Ui::cell(discount ? QString("%1 %").arg(discount) : QStringLiteral("—"), discount)});
        if (q.value(0).toInt() == keep)
            row = m_model->rowCount() - 1;
    }
    m_view->resizeColumnsToContents();
    m_footer->setText(QStringLiteral("Клієнтів у базі: %1").arg(m_model->rowCount()));
    if (row >= 0)
        m_view->selectRow(m_proxy->mapFromSource(m_model->index(row, 0)).row());
    showHistory();
}

int ClientsPage::selectedId() const
{
    const QModelIndex idx = m_view->currentIndex();
    return idx.isValid() ? m_proxy->index(idx.row(), 0).data(Ui::IdRole).toInt() : 0;
}

void ClientsPage::showHistory()
{
    m_history->setRowCount(0);
    const int id = selectedId();
    if (!id) {
        m_historyTitle->setText(QStringLiteral("Історія клієнта"));
        return;
    }
    m_historyTitle->setText(QStringLiteral("Історія: %1")
                                .arg(m_proxy->index(m_view->currentIndex().row(), 0).data().toString()));

    // бронювання та оренда в одному списку через UNION ALL
    QSqlQuery q;
    q.prepare("SELECT b.start_at AS d, 'Зал «' || h.name || '»', b.total, 'b:' || b.status FROM bookings b"
              " JOIN halls h ON h.id = b.hall_id WHERE b.client_id = ?"
              " UNION ALL"
              " SELECT r.start_date, 'Оренда реквізиту (' || (SELECT SUM(qty) FROM rental_items WHERE rental_id = r.id)"
              " || ' од.)', r.total + r.fine, 'r:' || r.status FROM rentals r WHERE r.client_id = ?"
              " ORDER BY d DESC");
    q.addBindValue(id);
    q.addBindValue(id);
    if (!q.exec())
        return;
    while (q.next()) {
        const int r = m_history->rowCount();
        m_history->insertRow(r);
        const QString s = q.value(3).toString();
        const QString statusText = s.startsWith("b:") ? Ui::bookingStatusText(s.mid(2)) : Ui::rentalStatusText(s.mid(2));
        m_history->setItem(r, 0, new QTableWidgetItem(Db::dateFromDb(q.value(0)).toString("dd.MM.yyyy")));
        m_history->setItem(r, 1, new QTableWidgetItem(q.value(1).toString()));
        m_history->setItem(r, 2, new QTableWidgetItem(Exporter::money(q.value(2).toLongLong())));
        m_history->setItem(r, 3, new QTableWidgetItem(statusText));
    }
    m_history->resizeColumnsToContents();
}

void ClientsPage::addClient()
{
    if (ClientDialog::createClient(this))
        refresh();
}

void ClientsPage::editSelected()
{
    const int id = selectedId();
    if (!id)
        return;
    ClientDialog dlg(id, this);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void ClientsPage::deleteSelected()
{
    const int id = selectedId();
    if (!id)
        return;
    const int links = Db::scalar("SELECT (SELECT COUNT(*) FROM bookings WHERE client_id = ?)"
                                 " + (SELECT COUNT(*) FROM rentals WHERE client_id = ?)", {id, id}).toInt();
    if (links > 0) {
        QMessageBox::information(this, QStringLiteral("Видалення"),
                                 QStringLiteral("У клієнта є %1 записів в історії бронювань та оренди.\n"
                                                "Щоб зберегти історію, такого клієнта видалити не можна.").arg(links));
        return;
    }
    if (!Ui::confirm(this, QStringLiteral("Видалення"), QStringLiteral("Видалити клієнта з бази?")))
        return;
    QString error;
    int rows = 0;
    if (!Db::exec("DELETE FROM clients WHERE id = ?", {id}, &error, &rows) || rows != 1) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error.isEmpty() ? QStringLiteral("Клієнта не знайдено.") : error);
        return;
    }
    Db::log(QStringLiteral("Видалено клієнта"), QStringLiteral("id %1").arg(id));
    refresh();
}

void ClientsPage::importCsv()
{
    const QString name = QFileDialog::getOpenFileName(this, QStringLiteral("Імпорт клієнтів"), QString(),
                                                      QStringLiteral("CSV (*.csv);;Усі файли (*)"));
    if (name.isEmpty())
        return;
    int count = 0;
    QString error;
    if (Db::importClients(name, &count, &error)) {
        QMessageBox::information(this, QStringLiteral("Імпорт"), QStringLiteral("Додано клієнтів: %1").arg(count));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("Імпорт не виконано"),
                             error + QStringLiteral("\n\nЖоден запис з файла не додано."));
    }
}

void ClientsPage::exportCsv()
{
    const QString name = QFileDialog::getSaveFileName(this, QStringLiteral("Експорт клієнтів"),
                                                      QStringLiteral("clients.csv"), QStringLiteral("CSV (*.csv)"));
    if (name.isEmpty())
        return;
    QString error;
    if (!Exporter::saveCsv(name, m_proxy, &error))
        QMessageBox::warning(this, QStringLiteral("Помилка експорту"), error);
}
