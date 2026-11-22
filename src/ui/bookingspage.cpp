#include "bookingspage.h"
#include "bookingdialog.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/session.h"
#include "theme.h"
#include "widgets.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

BookingsPage::BookingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *btnAdd = new QPushButton(Theme::icon("plus", Qt::white), QStringLiteral("Нове бронювання"));
    btnAdd->setObjectName("primary");
    connect(btnAdd, &QPushButton::clicked, this, &BookingsPage::addBooking);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Пошук: клієнт, телефон, зал..."));
    m_search->addAction(Theme::icon("search"), QLineEdit::LeadingPosition);
    m_search->setClearButtonEnabled(true);

    m_hall = new QComboBox;
    m_status = new QComboBox;
    m_status->addItem(QStringLiteral("Усі статуси"), QString());
    for (const char *code : {"new", "confirmed", "completed", "cancelled"})
        m_status->addItem(Ui::bookingStatusText(code), QString::fromLatin1(code));

    m_from = new QDateEdit(QDate::currentDate().addDays(-30));
    m_to = new QDateEdit(QDate::currentDate().addDays(30));
    for (QDateEdit *d : {m_from, m_to}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd.MM.yyyy");
    }

    auto *filters = new QHBoxLayout;
    filters->addWidget(m_search, 2);
    filters->addWidget(m_hall, 1);
    filters->addWidget(m_status, 1);
    filters->addWidget(new QLabel(QStringLiteral("з")));
    filters->addWidget(m_from);
    filters->addWidget(new QLabel(QStringLiteral("по")));
    filters->addWidget(m_to);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("№"), QStringLiteral("Дата"), QStringLiteral("Час"),
                                        QStringLiteral("Зал"), QStringLiteral("Клієнт"), QStringLiteral("Телефон"),
                                        QStringLiteral("Сума"), QStringLiteral("Сплачено"), QStringLiteral("Статус")});
    m_proxy = new TableProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterKeyColumn(-1);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_view = new QTableView;
    m_view->setModel(m_proxy);
    Ui::setupTable(m_view);
    m_view->setItemDelegateForColumn(8, new StatusDelegate(m_view));
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QTableView::doubleClicked, this, &BookingsPage::editSelected);

    // дії над вибраним бронюванням
    auto *btnEdit = new QPushButton(Theme::icon("edit"), QStringLiteral("Відкрити"));
    auto *btnConfirm = new QPushButton(Theme::icon("check"), QStringLiteral("Підтвердити"));
    auto *btnDone = new QPushButton(QStringLiteral("Завершити"));
    auto *btnCancel = new QPushButton(QStringLiteral("Скасувати"));
    auto *btnReceipt = new QPushButton(Theme::icon("print"), QStringLiteral("Квитанція"));
    auto *btnExport = new QPushButton(Theme::icon("download"), QStringLiteral("Експорт CSV"));
    auto *btnDelete = new QPushButton(Theme::icon("trash"), QString());
    btnDelete->setToolTip(QStringLiteral("Видалити бронювання"));
    btnDelete->setObjectName("danger");
    btnDelete->setVisible(Session::instance().isAdmin());

    connect(btnEdit, &QPushButton::clicked, this, &BookingsPage::editSelected);
    connect(btnConfirm, &QPushButton::clicked, this, [this] { setStatus("confirmed"); });
    connect(btnDone, &QPushButton::clicked, this, [this] { setStatus("completed"); });
    connect(btnCancel, &QPushButton::clicked, this, [this] { setStatus("cancelled"); });
    connect(btnReceipt, &QPushButton::clicked, this, &BookingsPage::printReceipt);
    connect(btnExport, &QPushButton::clicked, this, &BookingsPage::exportCsv);
    connect(btnDelete, &QPushButton::clicked, this, &BookingsPage::deleteSelected);

    connect(m_view, &QTableView::customContextMenuRequested, this, [=](const QPoint &pos) {
        if (!m_view->indexAt(pos).isValid())
            return;
        QMenu menu;
        menu.addAction(QStringLiteral("Відкрити"), this, &BookingsPage::editSelected);
        menu.addAction(QStringLiteral("Підтвердити"), this, [this] { setStatus("confirmed"); });
        menu.addAction(QStringLiteral("Завершити"), this, [this] { setStatus("completed"); });
        menu.addAction(QStringLiteral("Скасувати"), this, [this] { setStatus("cancelled"); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Квитанція"), this, &BookingsPage::printReceipt);
        menu.exec(m_view->viewport()->mapToGlobal(pos));
    });

    auto *actions = new QHBoxLayout;
    for (QPushButton *b : {btnEdit, btnConfirm, btnDone, btnCancel, btnReceipt})
        actions->addWidget(b);
    actions->addStretch();
    actions->addWidget(btnExport);
    actions->addWidget(btnDelete);

    m_footer = new QLabel;
    m_footer->setObjectName("pageSubtitle");

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);
    lay->addWidget(Ui::pageHeader(QStringLiteral("Бронювання"),
                                  QStringLiteral("Усі бронювання залів з фільтрами за датою, залом і статусом"), btnAdd));
    lay->addLayout(filters);
    lay->addWidget(m_view, 1);
    lay->addLayout(actions);
    lay->addWidget(m_footer);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_proxy->setFilterFixedString(t);
        m_footer->setText(QStringLiteral("Показано записів: %1").arg(m_proxy->rowCount()));
    });
    connect(m_hall, QOverload<int>::of(&QComboBox::activated), this, &BookingsPage::refresh);
    connect(m_status, QOverload<int>::of(&QComboBox::activated), this, &BookingsPage::refresh);
    connect(m_from, &QDateEdit::dateChanged, this, &BookingsPage::refresh);
    connect(m_to, &QDateEdit::dateChanged, this, &BookingsPage::refresh);
}

void BookingsPage::refresh()
{
    // список залів оновлюємо, зберігаючи вибір
    const QVariant hallSel = m_hall->currentData();
    m_hall->clear();
    m_hall->addItem(QStringLiteral("Усі зали"), 0);
    QSqlQuery h("SELECT id, name FROM halls ORDER BY id");
    while (h.next())
        m_hall->addItem(h.value(1).toString(), h.value(0));
    const int hallIdx = m_hall->findData(hallSel);
    m_hall->setCurrentIndex(hallIdx >= 0 ? hallIdx : 0);

    // умови фільтра складаються з фіксованих фрагментів, значення - лише параметрами
    QString sql = "SELECT b.id, b.start_at, b.end_at, h.name, c.full_name, c.phone, b.total, b.prepaid, b.status"
                  " FROM bookings b JOIN halls h ON h.id = b.hall_id JOIN clients c ON c.id = b.client_id"
                  " WHERE b.start_at >= ? AND b.start_at < ?";
    QVariantList args = {Db::toDb(m_from->date()), Db::toDb(m_to->date().addDays(1))};
    if (m_hall->currentData().toInt() > 0) {
        sql += " AND b.hall_id = ?";
        args << m_hall->currentData();
    }
    if (!m_status->currentData().toString().isEmpty()) {
        sql += " AND b.status = ?";
        args << m_status->currentData();
    }
    sql += " ORDER BY b.start_at DESC";

    QSqlQuery q;
    q.prepare(sql);
    for (const QVariant &a : args)
        q.addBindValue(a);

    m_model->removeRows(0, m_model->rowCount());
    qint64 sum = 0;
    if (q.exec()) {
        while (q.next()) {
            const QDateTime from = Db::dtFromDb(q.value(1));
            const QDateTime to = Db::dtFromDb(q.value(2));
            const QString status = q.value(8).toString();
            QStandardItem *id = Ui::cell(QString::number(q.value(0).toInt()), q.value(0).toInt());
            id->setData(q.value(0), Ui::IdRole);
            m_model->appendRow({id,
                                Ui::cell(from.toString("dd.MM.yyyy, ddd"), q.value(1)),
                                Ui::cell(from.toString("HH:mm") + "–" + to.toString("HH:mm")),
                                Ui::cell(q.value(3).toString()),
                                Ui::cell(q.value(4).toString()),
                                Ui::cell(q.value(5).toString()),
                                Ui::moneyCell(q.value(6).toLongLong()),
                                Ui::moneyCell(q.value(7).toLongLong()),
                                Ui::cell(Ui::bookingStatusText(status))});
            if (status != "cancelled")
                sum += q.value(6).toLongLong();
        }
    }
    m_view->resizeColumnsToContents();
    m_footer->setText(QStringLiteral("Знайдено бронювань: %1. Сума без скасованих: %2")
                          .arg(m_model->rowCount()).arg(Exporter::money(sum)));
}

int BookingsPage::selectedId() const
{
    const QModelIndex idx = m_view->currentIndex();
    if (!idx.isValid())
        return 0;
    return m_proxy->index(idx.row(), 0).data(Ui::IdRole).toInt();
}

void BookingsPage::addBooking()
{
    BookingDialog dlg(0, this);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void BookingsPage::editSelected()
{
    const int id = selectedId();
    if (!id) {
        QMessageBox::information(this, QStringLiteral("Бронювання"), QStringLiteral("Оберіть бронювання у таблиці."));
        return;
    }
    BookingDialog dlg(id, this);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void BookingsPage::setStatus(const QString &status)
{
    const int id = selectedId();
    if (!id)
        return;
    const QString current = Db::scalar("SELECT status FROM bookings WHERE id = ?", {id}).toString();
    if (current == status)
        return;
    if (current == "cancelled") {
        QMessageBox::information(this, QStringLiteral("Бронювання"),
                                 QStringLiteral("Скасоване бронювання можна відновити лише через форму редагування,\n"
                                                "де буде перевірено, чи вільний ще зал."));
        return;
    }
    if (status == "cancelled"
        && !Ui::confirm(this, QStringLiteral("Скасування"),
                        QStringLiteral("Скасувати бронювання №%1? Зал і реквізит стануть вільними.").arg(id)))
        return;

    QVariantList args = {status};
    QString sql = "UPDATE bookings SET status = ?";
    // при завершенні вважаємо, що клієнт розрахувався повністю
    if (status == "completed")
        sql += ", prepaid = total";
    sql += " WHERE id = ?";
    args << id;

    QString error;
    int rows = 0;
    if (!Db::exec(sql, args, &error, &rows) || rows != 1) {
        QMessageBox::warning(this, QStringLiteral("Помилка"),
                             error.isEmpty() ? QStringLiteral("Запис не знайдено.") : error);
        return;
    }
    Db::log(QStringLiteral("Зміна статусу бронювання"), QStringLiteral("№%1 -> %2").arg(id).arg(status));
    refresh();
}

void BookingsPage::deleteSelected()
{
    const int id = selectedId();
    if (!id)
        return;
    if (!Ui::confirm(this, QStringLiteral("Видалення"),
                     QStringLiteral("Видалити бронювання №%1 остаточно?\n"
                                    "Зазвичай краще скасувати бронювання, щоб зберегти історію.").arg(id)))
        return;
    QString error;
    int rows = 0;
    // рядки booking_props видаляються каскадно (ON DELETE CASCADE)
    if (!Db::exec("DELETE FROM bookings WHERE id = ?", {id}, &error, &rows) || rows != 1) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error.isEmpty() ? QStringLiteral("Запис не знайдено.") : error);
        return;
    }
    Db::log(QStringLiteral("Видалено бронювання"), QStringLiteral("№%1").arg(id));
    refresh();
}

void BookingsPage::printReceipt()
{
    const int id = selectedId();
    if (id)
        Exporter::printPreview(BookingDialog::receiptHtml(id), this);
}

void BookingsPage::exportCsv()
{
    const QString name = QFileDialog::getSaveFileName(this, QStringLiteral("Експорт бронювань"),
                                                      QStringLiteral("bookings_%1.csv").arg(QDate::currentDate().toString("yyyy-MM-dd")),
                                                      QStringLiteral("CSV (*.csv)"));
    if (name.isEmpty())
        return;
    QString error;
    if (Exporter::saveCsv(name, m_proxy, &error))
        QMessageBox::information(this, QStringLiteral("Експорт"), QStringLiteral("Збережено записів: %1").arg(m_proxy->rowCount()));
    else
        QMessageBox::warning(this, QStringLiteral("Помилка експорту"), error);
}
