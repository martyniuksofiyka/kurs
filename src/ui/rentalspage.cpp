#include "rentalspage.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/pricing.h"
#include "rentaldialog.h"
#include "theme.h"
#include "widgets.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

RentalsPage::RentalsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *btnAdd = new QPushButton(Theme::icon("plus", Qt::white), QStringLiteral("Видати реквізит"));
    btnAdd->setObjectName("primary");
    connect(btnAdd, &QPushButton::clicked, this, &RentalsPage::addRental);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Пошук: клієнт, телефон, реквізит..."));
    m_search->addAction(Theme::icon("search"), QLineEdit::LeadingPosition);
    m_search->setClearButtonEnabled(true);

    m_filter = new QComboBox;
    m_filter->addItem(QStringLiteral("Активні (видані)"), "active");
    m_filter->addItem(QStringLiteral("Прострочені"), "overdue");
    m_filter->addItem(QStringLiteral("Повернуті"), "returned");
    m_filter->addItem(QStringLiteral("Скасовані"), "cancelled");
    m_filter->addItem(QStringLiteral("Усі"), "all");

    auto *filters = new QHBoxLayout;
    filters->addWidget(m_search, 2);
    filters->addWidget(m_filter, 1);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("№"), QStringLiteral("Клієнт"), QStringLiteral("Телефон"),
                                        QStringLiteral("Видано"), QStringLiteral("Повернути до"),
                                        QStringLiteral("Реквізит"), QStringLiteral("Сума"), QStringLiteral("Застава"),
                                        QStringLiteral("Штраф"), QStringLiteral("Статус")});
    m_proxy = new TableProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterKeyColumn(-1);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_view = new QTableView;
    m_view->setModel(m_proxy);
    Ui::setupTable(m_view);
    m_view->setItemDelegateForColumn(9, new StatusDelegate(m_view));
    connect(m_view, &QTableView::doubleClicked, this, &RentalsPage::openSelected);

    auto *btnOpen = new QPushButton(Theme::icon("edit"), QStringLiteral("Відкрити"));
    auto *btnReturn = new QPushButton(Theme::icon("check"), QStringLiteral("Прийняти повернення"));
    auto *btnCancel = new QPushButton(QStringLiteral("Скасувати"));
    auto *btnAct = new QPushButton(Theme::icon("print"), QStringLiteral("Акт"));
    auto *btnExport = new QPushButton(Theme::icon("download"), QStringLiteral("Експорт CSV"));
    connect(btnOpen, &QPushButton::clicked, this, &RentalsPage::openSelected);
    connect(btnReturn, &QPushButton::clicked, this, &RentalsPage::returnSelected);
    connect(btnCancel, &QPushButton::clicked, this, &RentalsPage::cancelSelected);
    connect(btnAct, &QPushButton::clicked, this, [this] {
        if (const int id = selectedId())
            Exporter::printPreview(RentalDialog::actHtml(id), this);
    });
    connect(btnExport, &QPushButton::clicked, this, &RentalsPage::exportCsv);

    auto *actions = new QHBoxLayout;
    for (QPushButton *b : {btnOpen, btnReturn, btnCancel, btnAct})
        actions->addWidget(b);
    actions->addStretch();
    actions->addWidget(btnExport);

    m_footer = new QLabel;
    m_footer->setObjectName("pageSubtitle");

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);
    lay->addWidget(Ui::pageHeader(QStringLiteral("Оренда реквізиту"),
                                  QStringLiteral("Видача реквізиту на винос, повернення та штрафи за прострочення"), btnAdd));
    lay->addLayout(filters);
    lay->addWidget(m_view, 1);
    lay->addLayout(actions);
    lay->addWidget(m_footer);

    connect(m_search, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_filter, QOverload<int>::of(&QComboBox::activated), this, &RentalsPage::refresh);
}

void RentalsPage::refresh()
{
    const QString filter = m_filter->currentData().toString();
    const QString today = Db::toDb(QDate::currentDate());

    QString sql = "SELECT r.id, c.full_name, c.phone, r.start_date, r.end_date,"
                  " GROUP_CONCAT(p.name || ' ×' || ri.qty, ', '), r.total, r.deposit, r.fine, r.status"
                  " FROM rentals r JOIN clients c ON c.id = r.client_id"
                  " JOIN rental_items ri ON ri.rental_id = r.id JOIN props p ON p.id = ri.prop_id";
    QVariantList args;
    if (filter == "active") {
        sql += " WHERE r.status = 'active'";
    } else if (filter == "overdue") {
        sql += " WHERE r.status = 'active' AND r.end_date < ?";
        args << today;
    } else if (filter != "all") {
        sql += " WHERE r.status = ?";
        args << filter;
    }
    sql += " GROUP BY r.id ORDER BY r.end_date";

    QSqlQuery q;
    q.prepare(sql);
    for (const QVariant &a : args)
        q.addBindValue(a);

    m_model->removeRows(0, m_model->rowCount());
    int overdue = 0;
    if (q.exec()) {
        while (q.next()) {
            const QDate end = Db::dateFromDb(q.value(4));
            const QString status = q.value(9).toString();
            const bool late = status == "active" && end < QDate::currentDate();
            overdue += late;
            QStandardItem *id = Ui::cell(QString::number(q.value(0).toInt()), q.value(0).toInt());
            id->setData(q.value(0), Ui::IdRole);
            m_model->appendRow({id,
                                Ui::cell(q.value(1).toString()),
                                Ui::cell(q.value(2).toString()),
                                Ui::cell(Db::dateFromDb(q.value(3)).toString("dd.MM.yyyy"), q.value(3)),
                                Ui::cell(end.toString("dd.MM.yyyy"), q.value(4)),
                                Ui::cell(q.value(5).toString()),
                                Ui::moneyCell(q.value(6).toLongLong()),
                                Ui::moneyCell(q.value(7).toLongLong()),
                                Ui::moneyCell(q.value(8).toLongLong()),
                                Ui::cell(Ui::rentalStatusText(status, late))});
        }
    }
    m_view->resizeColumnsToContents();
    m_footer->setText(QStringLiteral("Записів: %1%2").arg(m_model->rowCount())
                          .arg(overdue ? QStringLiteral(", прострочено: %1").arg(overdue) : QString()));
}

int RentalsPage::selectedId() const
{
    const QModelIndex idx = m_view->currentIndex();
    return idx.isValid() ? m_proxy->index(idx.row(), 0).data(Ui::IdRole).toInt() : 0;
}

void RentalsPage::addRental()
{
    RentalDialog dlg(0, this);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void RentalsPage::openSelected()
{
    const int id = selectedId();
    if (!id)
        return;
    RentalDialog dlg(id, this);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void RentalsPage::returnSelected()
{
    const int id = selectedId();
    if (!id)
        return;

    QSqlQuery q;
    q.prepare("SELECT end_date, status, total, deposit FROM rentals WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return;
    if (q.value(1).toString() != "active") {
        QMessageBox::information(this, QStringLiteral("Повернення"), QStringLiteral("Ця оренда вже закрита."));
        return;
    }

    // штраф рахується окремо для кожного найменування
    const QDate planned = Db::dateFromDb(q.value(0));
    const QDate today = QDate::currentDate();
    Pricing::Money fine = 0;
    QSqlQuery items;
    items.prepare("SELECT qty, price_per_day FROM rental_items WHERE rental_id = ?");
    items.addBindValue(id);
    if (items.exec()) {
        while (items.next())
            fine += Pricing::lateFine(items.value(1).toLongLong(), items.value(0).toInt(), planned, today);
    }

    QString text = QStringLiteral("Прийняти повернення реквізиту за орендою №%1?\n\n").arg(id);
    if (fine > 0)
        text += QStringLiteral("Прострочення: %1 дн.\nШтраф: %2\n").arg(planned.daysTo(today)).arg(Exporter::money(fine));
    text += QStringLiteral("Повернути клієнту заставу: %1").arg(Exporter::money(q.value(3).toLongLong()));
    if (!Ui::confirm(this, QStringLiteral("Повернення реквізиту"), text))
        return;

    QString error;
    int rows = 0;
    if (!Db::exec("UPDATE rentals SET status = 'returned', returned_at = ?, fine = ? WHERE id = ? AND status = 'active'",
                  {Db::toDb(QDateTime::currentDateTime()), fine, id}, &error, &rows) || rows != 1) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error.isEmpty() ? QStringLiteral("Оренду не знайдено.") : error);
        return;
    }
    Db::log(QStringLiteral("Повернення реквізиту"), QStringLiteral("оренда №%1, штраф %2").arg(id).arg(fine));
    refresh();
}

void RentalsPage::cancelSelected()
{
    const int id = selectedId();
    if (!id)
        return;
    const QString status = Db::scalar("SELECT status FROM rentals WHERE id = ?", {id}).toString();
    if (status != "active") {
        QMessageBox::information(this, QStringLiteral("Скасування"), QStringLiteral("Можна скасувати лише активну оренду."));
        return;
    }
    if (!Ui::confirm(this, QStringLiteral("Скасування"),
                     QStringLiteral("Скасувати оренду №%1? Реквізит повернеться на склад без оплати.").arg(id)))
        return;
    QString error;
    int rows = 0;
    if (!Db::exec("UPDATE rentals SET status = 'cancelled' WHERE id = ? AND status = 'active'", {id}, &error, &rows)
        || rows != 1) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error.isEmpty() ? QStringLiteral("Оренду не знайдено.") : error);
        return;
    }
    Db::log(QStringLiteral("Скасовано оренду"), QStringLiteral("№%1").arg(id));
    refresh();
}

void RentalsPage::exportCsv()
{
    const QString name = QFileDialog::getSaveFileName(this, QStringLiteral("Експорт оренди"),
                                                      QStringLiteral("rentals_%1.csv").arg(QDate::currentDate().toString("yyyy-MM-dd")),
                                                      QStringLiteral("CSV (*.csv)"));
    if (name.isEmpty())
        return;
    QString error;
    if (!Exporter::saveCsv(name, m_proxy, &error))
        QMessageBox::warning(this, QStringLiteral("Помилка експорту"), error);
}
