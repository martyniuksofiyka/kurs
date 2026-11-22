#ifndef SESSION_H
#define SESSION_H

#include <QString>
#include <QTime>

// Дані про користувача, який увійшов у систему, та налаштування студії
class Session
{
public:
    static Session &instance();

    int userId = 0;
    QString login;
    QString fullName;
    QString role;          // admin або manager

    bool isAdmin() const { return role == QLatin1String("admin"); }

    QTime openTime() const;
    QTime closeTime() const;
    void setWorkingHours(const QTime &open, const QTime &close);

    QString studioName() const;
    void setStudioName(const QString &name);

private:
    Session() = default;
};

#endif // SESSION_H
