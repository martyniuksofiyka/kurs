#include "userspage.h"
#include "core/database.h"
#include "core/session.h"
#include "theme.h"
#include "widgets.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

UsersPage::UsersPage(QWidget *parent)
    : QWidget(parent)
{
    auto *btnAdd = new QPushButton(Theme::icon("plus", Qt::white), QStringLiteral("Новий користувач"));
    btnAdd->setObjectName("primary");
    connect(btnAdd, &QPushButton::clicked, this, &UsersPage::addUser);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("Логін"), QStringLiteral("ПІБ"), QStringLiteral("Роль"),
                                        QStringLiteral("Стан")});
    m_view = new QTableView;
    m_view->setModel(m_model);
    Ui::setupTable(m_view);
    m_view->setSortingEnabled(false);
    m_view->setItemDelegateForColumn(3, new StatusDelegate(m_view));
    connect(m_view, &QTableView::doubleClicked, this, &UsersPage::editSelected);

    auto *btnEdit = new QPushButton(Theme::icon("edit"), QStringLiteral("Редагувати"));
    auto *btnToggle = new QPushButton(QStringLiteral("Заблокувати / розблокувати"));
    auto *btnPass = new QPushButton(Theme::icon("lock"), QStringLiteral("Скинути пароль"));
    connect(btnEdit, &QPushButton::clicked, this, &UsersPage::editSelected);
    connect(btnToggle, &QPushButton::clicked, this, &UsersPage::toggleActive);
    connect(btnPass, &QPushButton::clicked, this, &UsersPage::resetPassword);

    auto *actions = new QHBoxLayout;
    actions->addWidget(btnEdit);
    actions->addWidget(btnToggle);
    actions->addWidget(btnPass);
    actions->addStretch();

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);
    lay->addWidget(Ui::pageHeader(QStringLiteral("Користувачі"),
                                  QStringLiteral("Облікові записи працівників студії та їхні ролі"), btnAdd));
    lay->addWidget(m_view, 1);
    lay->addLayout(actions);
}

void UsersPage::refresh()
{
    m_model->removeRows(0, m_model->rowCount());
    QSqlQuery q("SELECT id, login, full_name, role, active FROM users ORDER BY id");
    while (q.next()) {
        QStandardItem *login = Ui::cell(q.value(1).toString());
        login->setData(q.value(0), Ui::IdRole);
        m_model->appendRow({login, Ui::cell(q.value(2).toString()),
                            Ui::cell(q.value(3).toString() == "admin" ? QStringLiteral("Адміністратор")
                                                                      : QStringLiteral("Менеджер")),
                            Ui::cell(q.value(4).toBool() ? QStringLiteral("Активний") : QStringLiteral("Заблокований"))});
    }
    m_view->resizeColumnsToContents();
}

int UsersPage::selectedId() const
{
    const QModelIndex idx = m_view->currentIndex();
    return idx.isValid() ? m_model->index(idx.row(), 0).data(Ui::IdRole).toInt() : 0;
}

void UsersPage::addUser()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Новий користувач"));
    auto *login = new QLineEdit;
    login->setMaxLength(32);
    auto *name = new QLineEdit;
    name->setMaxLength(100);
    auto *role = new QComboBox;
    role->addItem(QStringLiteral("Менеджер"), "manager");
    role->addItem(QStringLiteral("Адміністратор"), "admin");
    auto *pass = new QLineEdit;
    pass->setEchoMode(QLineEdit::Password);

    auto *form = new QFormLayout(&dlg);
    form->addRow(QStringLiteral("Логін:"), login);
    form->addRow(QStringLiteral("ПІБ:"), name);
    form->addRow(QStringLiteral("Роль:"), role);
    form->addRow(QStringLiteral("Пароль:"), pass);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, [&] {
        static const QRegularExpression loginRe("^[A-Za-z0-9_.]{3,32}$");
        QString problem;
        if (!loginRe.match(login->text().trimmed()).hasMatch())
            problem = QStringLiteral("Логін: 3–32 латинські літери, цифри, '_' або '.'.");
        else if (name->text().trimmed().isEmpty())
            problem = QStringLiteral("Вкажіть ПІБ.");
        else if (pass->text().length() < 6)
            problem = QStringLiteral("Пароль має містити щонайменше 6 символів.");
        if (!problem.isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("Помилка"), problem);
            return;
        }
        QString error;
        if (!Db::addUser(login->text().trimmed(), name->text().trimmed(), role->currentData().toString(),
                         pass->text(), &error)) {
            QMessageBox::warning(&dlg, QStringLiteral("Помилка"),
                                 error.contains("UNIQUE") ? QStringLiteral("Такий логін уже існує.") : error);
            return;
        }
        Db::log(QStringLiteral("Створено користувача"), login->text().trimmed());
        dlg.accept();
    });
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void UsersPage::editSelected()
{
    const int id = selectedId();
    if (!id)
        return;
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Редагування користувача"));
    auto *name = new QLineEdit(m_model->index(m_view->currentIndex().row(), 1).data().toString());
    auto *role = new QComboBox;
    role->addItem(QStringLiteral("Менеджер"), "manager");
    role->addItem(QStringLiteral("Адміністратор"), "admin");
    role->setCurrentIndex(role->findData(Db::scalar("SELECT role FROM users WHERE id = ?", {id})));
    // власну роль змінювати не можна, щоб не втратити доступ адміністратора
    role->setEnabled(id != Session::instance().userId);

    auto *form = new QFormLayout(&dlg);
    form->addRow(QStringLiteral("ПІБ:"), name);
    form->addRow(QStringLiteral("Роль:"), role);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted || name->text().trimmed().isEmpty())
        return;
    QString error;
    if (!Db::exec("UPDATE users SET full_name = ?, role = ? WHERE id = ?",
                  {name->text().trimmed(), role->currentData(), id}, &error))
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
    refresh();
}

void UsersPage::toggleActive()
{
    const int id = selectedId();
    if (!id)
        return;
    if (id == Session::instance().userId) {
        QMessageBox::information(this, QStringLiteral("Користувачі"), QStringLiteral("Не можна заблокувати власний обліковий запис."));
        return;
    }
    QString error;
    if (!Db::exec("UPDATE users SET active = 1 - active WHERE id = ?", {id}, &error))
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
    refresh();
}

void UsersPage::resetPassword()
{
    const int id = selectedId();
    if (!id)
        return;
    bool ok = false;
    const QString pass = QInputDialog::getText(this, QStringLiteral("Скидання пароля"),
                                               QStringLiteral("Новий пароль (не менше 6 символів):"),
                                               QLineEdit::Password, QString(), &ok);
    if (!ok)
        return;
    if (pass.length() < 6) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), QStringLiteral("Пароль занадто короткий."));
        return;
    }
    if (Db::setPassword(id, pass)) {
        Db::log(QStringLiteral("Скинуто пароль користувача"), QStringLiteral("id %1").arg(id));
        QMessageBox::information(this, QStringLiteral("Скидання пароля"), QStringLiteral("Пароль змінено."));
    }
}
