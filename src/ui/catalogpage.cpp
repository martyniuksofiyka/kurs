#include "catalogpage.h"
#include "core/database.h"
#include "core/exporter.h"
#include "core/pricing.h"
#include "core/session.h"
#include "theme.h"
#include "widgets.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

static QTableView *makeView(QStandardItemModel *model, TableProxy *proxy)
{
    proxy->setSourceModel(model);
    auto *view = new QTableView;
    view->setModel(proxy);
    Ui::setupTable(view);
    return view;
}

CatalogPage::CatalogPage(QWidget *parent)
    : QWidget(parent)
{
    m_halls = new QStandardItemModel(this);
    m_halls->setHorizontalHeaderLabels({QStringLiteral("Зал"), QStringLiteral("Площа"), QStringLiteral("Ціна за год"),
                                        QStringLiteral("Бронювань за 30 днів"), QStringLiteral("Опис"),
                                        QStringLiteral("Стан")});
    m_props = new QStandardItemModel(this);
    m_props->setHorizontalHeaderLabels({QStringLiteral("Найменування"), QStringLiteral("Категорія"),
                                        QStringLiteral("Усього"), QStringLiteral("Зараз вільно"),
                                        QStringLiteral("Ціна за год"), QStringLiteral("Ціна за добу"),
                                        QStringLiteral("Застава"), QStringLiteral("Стан")});
    m_hallsProxy = new TableProxy(this);
    m_propsProxy = new TableProxy(this);
    m_hallsView = makeView(m_halls, m_hallsProxy);
    m_propsView = makeView(m_props, m_propsProxy);
    m_hallsView->setItemDelegateForColumn(5, new StatusDelegate(m_hallsView));
    m_propsView->setItemDelegateForColumn(7, new StatusDelegate(m_propsView));
    connect(m_hallsView, &QTableView::doubleClicked, this, &CatalogPage::editSelected);
    connect(m_propsView, &QTableView::doubleClicked, this, &CatalogPage::editSelected);

    m_tabs = new QTabWidget;
    m_tabs->addTab(m_hallsView, Theme::icon("camera"), QStringLiteral("Зали"));
    m_tabs->addTab(m_propsView, Theme::icon("box"), QStringLiteral("Реквізит"));

    auto *btnAdd = new QPushButton(Theme::icon("plus", Qt::white), QStringLiteral("Додати"));
    btnAdd->setObjectName("primary");
    auto *btnEdit = new QPushButton(Theme::icon("edit"), QStringLiteral("Редагувати"));
    auto *btnToggle = new QPushButton(QStringLiteral("Вивести з використання / повернути"));
    connect(btnAdd, &QPushButton::clicked, this, &CatalogPage::add);
    connect(btnEdit, &QPushButton::clicked, this, &CatalogPage::editSelected);
    connect(btnToggle, &QPushButton::clicked, this, &CatalogPage::toggleActive);

    // змінювати довідники може лише адміністратор
    const bool admin = Session::instance().isAdmin();
    for (QPushButton *b : {btnAdd, btnEdit, btnToggle})
        b->setEnabled(admin);

    auto *actions = new QHBoxLayout;
    actions->addWidget(btnEdit);
    actions->addWidget(btnToggle);
    actions->addStretch();
    if (!admin) {
        auto *hint = new QLabel(QStringLiteral("Редагування довідників доступне адміністратору"));
        hint->setObjectName("pageSubtitle");
        actions->addWidget(hint);
    }

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);
    lay->addWidget(Ui::pageHeader(QStringLiteral("Зали та реквізит"),
                                  QStringLiteral("Довідники студії: ціни, кількість, застава"), btnAdd));
    lay->addWidget(m_tabs, 1);
    lay->addLayout(actions);
}

void CatalogPage::refresh()
{
    m_halls->removeRows(0, m_halls->rowCount());
    QSqlQuery q;
    q.prepare("SELECT h.id, h.name, h.area, h.price_per_hour, h.description, h.active, h.color,"
              " (SELECT COUNT(*) FROM bookings b WHERE b.hall_id = h.id AND b.status <> 'cancelled'"
              "  AND b.start_at >= ?) FROM halls h ORDER BY h.id");
    q.addBindValue(Db::toDb(QDate::currentDate().addDays(-30)));
    if (q.exec()) {
        while (q.next()) {
            QPixmap dot(12, 12);
            dot.fill(QColor(q.value(6).toString()));
            QStandardItem *name = Ui::cell(q.value(1).toString());
            name->setIcon(QIcon(dot));
            name->setData(q.value(0), Ui::IdRole);
            m_halls->appendRow({name,
                                Ui::cell(QStringLiteral("%1 м²").arg(q.value(2).toDouble()), q.value(2)),
                                Ui::moneyCell(q.value(3).toLongLong()),
                                Ui::cell(QString::number(q.value(7).toInt()), q.value(7)),
                                Ui::cell(q.value(4).toString()),
                                Ui::cell(q.value(5).toBool() ? QStringLiteral("Активний") : QStringLiteral("Неактивний"))});
        }
    }
    m_hallsView->resizeColumnsToContents();
    m_hallsView->setColumnWidth(4, 360);

    m_props->removeRows(0, m_props->rowCount());
    const QDateTime now = QDateTime::currentDateTime();
    q.exec("SELECT id, name, category, total_qty, price_per_hour, price_per_day, deposit, active"
           " FROM props ORDER BY category, name");
    while (q.next()) {
        const int id = q.value(0).toInt();
        const int free = Db::propAvailable(id, now, now.addSecs(60));
        QStandardItem *name = Ui::cell(q.value(1).toString());
        name->setData(id, Ui::IdRole);
        m_props->appendRow({name, Ui::cell(q.value(2).toString()),
                            Ui::cell(QString::number(q.value(3).toInt()), q.value(3)),
                            Ui::cell(QString::number(free), free),
                            Ui::moneyCell(q.value(4).toLongLong()), Ui::moneyCell(q.value(5).toLongLong()),
                            Ui::moneyCell(q.value(6).toLongLong()),
                            Ui::cell(q.value(7).toBool() ? QStringLiteral("Активний") : QStringLiteral("Неактивний"))});
    }
    m_propsView->resizeColumnsToContents();
}

bool CatalogPage::hallsTab() const
{
    return m_tabs->currentIndex() == 0;
}

int CatalogPage::selectedId() const
{
    QTableView *view = hallsTab() ? m_hallsView : m_propsView;
    const QModelIndex idx = view->currentIndex();
    return idx.isValid() ? view->model()->index(idx.row(), 0).data(Ui::IdRole).toInt() : 0;
}

void CatalogPage::add()
{
    if (hallsTab() ? editHall(0) : editProp(0))
        refresh();
}

void CatalogPage::editSelected()
{
    if (!Session::instance().isAdmin())
        return;
    const int id = selectedId();
    if (id && (hallsTab() ? editHall(id) : editProp(id)))
        refresh();
}

void CatalogPage::toggleActive()
{
    const int id = selectedId();
    if (!id)
        return;
    // фізично не видаляємо, бо на зал/реквізит посилається історія бронювань
    const QString table = hallsTab() ? "halls" : "props";
    QString error;
    int rows = 0;
    if (!Db::exec("UPDATE " + table + " SET active = 1 - active WHERE id = ?", {id}, &error, &rows) || rows != 1) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    refresh();
}

bool CatalogPage::editHall(int id)
{
    QDialog dlg(this);
    dlg.setWindowTitle(id ? QStringLiteral("Редагування залу") : QStringLiteral("Новий зал"));
    dlg.setMinimumWidth(460);

    auto *name = new QLineEdit;
    name->setMaxLength(60);
    auto *area = new QDoubleSpinBox;
    area->setRange(1, 1000);
    area->setSuffix(QStringLiteral(" м²"));
    auto *price = new QDoubleSpinBox;
    price->setRange(1, 100000);
    price->setDecimals(2);
    price->setSuffix(QStringLiteral(" грн"));
    auto *descr = new QPlainTextEdit;
    descr->setMaximumHeight(90);
    QColor color("#4f7cff");
    auto *colorBtn = new QPushButton;

    auto updateColor = [&] {
        colorBtn->setText(color.name());
        colorBtn->setStyleSheet(QString("background:%1; color:white;").arg(color.name()));
    };

    if (id) {
        QSqlQuery q;
        q.prepare("SELECT name, area, price_per_hour, description, color FROM halls WHERE id = ?");
        q.addBindValue(id);
        if (q.exec() && q.next()) {
            name->setText(q.value(0).toString());
            area->setValue(q.value(1).toDouble());
            price->setValue(Pricing::toHryvnia(q.value(2).toLongLong()));
            descr->setPlainText(q.value(3).toString());
            color = QColor(q.value(4).toString());
        }
    }
    updateColor();
    connect(colorBtn, &QPushButton::clicked, &dlg, [&] {
        const QColor c = QColorDialog::getColor(color, &dlg, QStringLiteral("Колір залу в розкладі"));
        if (c.isValid()) {
            color = c;
            updateColor();
        }
    });

    auto *form = new QFormLayout(&dlg);
    form->addRow(QStringLiteral("Назва*:"), name);
    form->addRow(QStringLiteral("Площа:"), area);
    form->addRow(QStringLiteral("Ціна за годину*:"), price);
    form->addRow(QStringLiteral("Колір у розкладі:"), colorBtn);
    form->addRow(QStringLiteral("Опис:"), descr);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, [&] {
        if (name->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("Помилка"), QStringLiteral("Вкажіть назву залу."));
            return;
        }
        QString error;
        const QVariantList v = {name->text().trimmed(), area->value(), Pricing::fromHryvnia(price->value()),
                                descr->toPlainText().trimmed(), color.name()};
        bool ok;
        if (id) {
            QVariantList args = v;
            args << id;
            ok = Db::exec("UPDATE halls SET name = ?, area = ?, price_per_hour = ?, description = ?, color = ? WHERE id = ?",
                          args, &error);
        } else {
            ok = Db::exec("INSERT INTO halls (name, area, price_per_hour, description, color) VALUES (?, ?, ?, ?, ?)",
                          v, &error);
        }
        if (!ok) {
            if (error.contains("UNIQUE"))
                error = QStringLiteral("Зал з такою назвою вже існує.");
            QMessageBox::warning(&dlg, QStringLiteral("Не збережено"), error);
            return;
        }
        dlg.accept();
    });
    return dlg.exec() == QDialog::Accepted;
}

bool CatalogPage::editProp(int id)
{
    QDialog dlg(this);
    dlg.setWindowTitle(id ? QStringLiteral("Редагування реквізиту") : QStringLiteral("Новий реквізит"));
    dlg.setMinimumWidth(460);

    auto *name = new QLineEdit;
    name->setMaxLength(80);
    auto *category = new QComboBox;
    category->setEditable(true);
    QSqlQuery c("SELECT DISTINCT category FROM props ORDER BY category");
    while (c.next())
        category->addItem(c.value(0).toString());
    auto *qty = new QSpinBox;
    qty->setRange(0, 1000);
    auto money = [] {
        auto *s = new QDoubleSpinBox;
        s->setRange(0, 100000);
        s->setDecimals(2);
        s->setSuffix(QStringLiteral(" грн"));
        return s;
    };
    QDoubleSpinBox *perHour = money();
    QDoubleSpinBox *perDay = money();
    QDoubleSpinBox *deposit = money();
    auto *condition = new QComboBox;
    condition->setEditable(true);
    condition->addItems({QStringLiteral("Новий"), QStringLiteral("Добрий"), QStringLiteral("Задовільний"),
                         QStringLiteral("Потребує ремонту")});

    if (id) {
        QSqlQuery q;
        q.prepare("SELECT name, category, total_qty, price_per_hour, price_per_day, deposit, condition FROM props WHERE id = ?");
        q.addBindValue(id);
        if (q.exec() && q.next()) {
            name->setText(q.value(0).toString());
            category->setCurrentText(q.value(1).toString());
            qty->setValue(q.value(2).toInt());
            perHour->setValue(Pricing::toHryvnia(q.value(3).toLongLong()));
            perDay->setValue(Pricing::toHryvnia(q.value(4).toLongLong()));
            deposit->setValue(Pricing::toHryvnia(q.value(5).toLongLong()));
            condition->setCurrentText(q.value(6).toString());
        }
    }

    auto *form = new QFormLayout(&dlg);
    form->addRow(QStringLiteral("Найменування*:"), name);
    form->addRow(QStringLiteral("Категорія*:"), category);
    form->addRow(QStringLiteral("Кількість:"), qty);
    form->addRow(QStringLiteral("Ціна за годину:"), perHour);
    form->addRow(QStringLiteral("Ціна за добу:"), perDay);
    form->addRow(QStringLiteral("Застава:"), deposit);
    form->addRow(QStringLiteral("Стан:"), condition);
    auto *hint = new QLabel(QStringLiteral("Ціна 0 означає, що цей спосіб оренди недоступний."));
    hint->setObjectName("hint");
    form->addRow(hint);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, [&] {
        QString problem;
        if (name->text().trimmed().isEmpty() || category->currentText().trimmed().isEmpty())
            problem = QStringLiteral("Вкажіть найменування та категорію.");
        else if (perHour->value() == 0 && perDay->value() == 0)
            problem = QStringLiteral("Вкажіть хоча б одну ціну (за годину або за добу).");
        else if (id) {
            // не можна зменшити кількість нижче тієї, що зараз видана
            const QDateTime now = QDateTime::currentDateTime();
            const int total = Db::scalar("SELECT total_qty FROM props WHERE id = ?", {id}).toInt();
            const int busy = total - Db::propAvailable(id, now, now.addSecs(60));
            if (qty->value() < busy)
                problem = QStringLiteral("Зараз в оренді %1 од., кількість не може бути меншою.").arg(busy);
        }
        if (!problem.isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("Помилка"), problem);
            return;
        }
        const QVariantList v = {name->text().trimmed(), category->currentText().trimmed(), qty->value(),
                                Pricing::fromHryvnia(perHour->value()), Pricing::fromHryvnia(perDay->value()),
                                Pricing::fromHryvnia(deposit->value()), condition->currentText().trimmed()};
        QString error;
        bool ok;
        if (id) {
            QVariantList args = v;
            args << id;
            ok = Db::exec("UPDATE props SET name = ?, category = ?, total_qty = ?, price_per_hour = ?, price_per_day = ?,"
                          " deposit = ?, condition = ? WHERE id = ?", args, &error);
        } else {
            ok = Db::exec("INSERT INTO props (name, category, total_qty, price_per_hour, price_per_day, deposit, condition)"
                          " VALUES (?, ?, ?, ?, ?, ?, ?)", v, &error);
        }
        if (!ok) {
            QMessageBox::warning(&dlg, QStringLiteral("Не збережено"), error);
            return;
        }
        dlg.accept();
    });
    return dlg.exec() == QDialog::Accepted;
}
