#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);

private slots:
    void tryLogin();

private:
    QLineEdit *m_login;
    QLineEdit *m_password;
    QLabel *m_error;
    int m_attempts = 0;
};

#endif // LOGINDIALOG_H
