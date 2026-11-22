#include "logindialog.h"
#include "core/database.h"
#include "core/session.h"
#include "theme.h"

#include <QAction>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Вхід у систему"));
    setObjectName("loginDialog");
    setFixedWidth(380);

    auto *logo = new QLabel;
    logo->setPixmap(Theme::icon("camera", Theme::Accent).pixmap(48, 48));
    logo->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(Session::instance().studioName());
    title->setObjectName("loginTitle");
    title->setAlignment(Qt::AlignCenter);
    auto *sub = new QLabel(QStringLiteral("Система бронювання та оренди реквізиту"));
    sub->setObjectName("pageSubtitle");
    sub->setAlignment(Qt::AlignCenter);

    m_login = new QLineEdit;
    m_login->setPlaceholderText(QStringLiteral("Логін"));
    m_login->addAction(Theme::icon("user"), QLineEdit::LeadingPosition);
    m_login->setText(QSettings().value("login/last").toString());

    m_password = new QLineEdit;
    m_password->setPlaceholderText(QStringLiteral("Пароль"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->addAction(Theme::icon("lock"), QLineEdit::LeadingPosition);

    m_error = new QLabel;
    m_error->setObjectName("errorLabel");
    m_error->setAlignment(Qt::AlignCenter);
    m_error->hide();

    auto *btn = new QPushButton(QStringLiteral("Увійти"));
    btn->setObjectName("primary");
    btn->setDefault(true);
    connect(btn, &QPushButton::clicked, this, &LoginDialog::tryLogin);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(10);
    lay->addWidget(logo);
    lay->addWidget(title);
    lay->addWidget(sub);
    lay->addSpacing(12);
    lay->addWidget(m_login);
    lay->addWidget(m_password);
    lay->addWidget(m_error);
    lay->addSpacing(4);
    lay->addWidget(btn);

    if (m_login->text().isEmpty())
        m_login->setFocus();
    else
        m_password->setFocus();
}

void LoginDialog::tryLogin()
{
    if (m_login->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        m_error->setText(QStringLiteral("Введіть логін і пароль"));
        m_error->show();
        return;
    }

    if (Db::authenticate(m_login->text(), m_password->text())) {
        QSettings().setValue("login/last", m_login->text().trimmed());
        accept();
        return;
    }

    ++m_attempts;
    m_password->clear();
    m_error->setText(QStringLiteral("Невірний логін або пароль"));
    m_error->show();

    // після трьох невдалих спроб блокуємо форму на кілька секунд
    if (m_attempts % 3 == 0) {
        setEnabled(false);
        m_error->setText(QStringLiteral("Забагато спроб. Зачекайте 5 секунд"));
        QTimer::singleShot(5000, this, [this] {
            setEnabled(true);
            m_error->setText(QStringLiteral("Спробуйте ще раз"));
            m_password->setFocus();
        });
    }
}
