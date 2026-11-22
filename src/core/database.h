#ifndef DATABASE_H
#define DATABASE_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariant>

#include <functional>

#include "scheduling.h"

// Модуль доступу до даних: підключення до SQLite, створення схеми,
// навчальні дані та запити, які використовуються в кількох вікнах.
// Значення користувача завжди передаються в SQL як параметри.
namespace Db {

extern const char *DateTimeFmt;
extern const char *DateFmt;

QString toDb(const QDateTime &dt);
QString toDb(const QDate &d);
QDateTime dtFromDb(const QVariant &v);
QDate dateFromDb(const QVariant &v);

// шлях до файла БД: заданий параметром --db або стандартний каталог даних
QString defaultPath();
QString path();

bool open(const QString &fileName, QString *error = nullptr);
void close();
bool createSchema(QString *error = nullptr);
bool isEmpty();
bool seedDemoData(QString *error = nullptr);
// видаляє всі дані й заповнює базу навчальним набором
bool resetDemoData(QString *error = nullptr);

// резервна копія засобами SQLite (VACUUM INTO)
bool backup(const QString &fileName, QString *error = nullptr);

// виконує дію в транзакції; при помилці виконує відкат
bool transaction(const std::function<bool(QString *)> &action, QString *error = nullptr);

QString lastError();

// журнал роботи програми (дія, час, опис помилки - без паролів)
void log(const QString &action, const QString &details = QString());

// --- користувачі ---
QString hashPassword(const QString &password, const QString &salt);
QString makeSalt();
bool authenticate(const QString &login, const QString &password);
bool addUser(const QString &login, const QString &fullName, const QString &role,
             const QString &password, QString *error = nullptr);
bool setPassword(int userId, const QString &password);

// --- бронювання та оренда ---
// id бронювання, з яким є накладка, або 0
int hallConflict(int hallId, const QDateTime &from, const QDateTime &to, int ignoreBookingId = 0);
QList<Scheduling::Interval> hallBusy(int hallId, const QDate &date, int ignoreBookingId = 0);

// інтервали, у які реквізит зайнятий (бронювання + оренда)
QList<Scheduling::Interval> propBusy(int propId, int ignoreBookingId = 0, int ignoreRentalId = 0);
int propAvailable(int propId, const QDateTime &from, const QDateTime &to,
                  int ignoreBookingId = 0, int ignoreRentalId = 0);

int completedVisits(int clientId);

// --- клієнти ---
// нормалізує номер до вигляду +380XXXXXXXXX; повертає порожній рядок, якщо номер некоректний
QString normalizePhone(const QString &phone);
// імпорт клієнтів з CSV: усі рядки додаються в одній транзакції
// або не додається жоден, якщо в файлі є помилка
bool importClients(const QString &fileName, int *imported, QString *error);

// --- допоміжні ---
QVariant scalar(const QString &sql, const QVariantList &args = {});
bool exec(const QString &sql, const QVariantList &args = {}, QString *error = nullptr,
          int *rowsAffected = nullptr);

}

#endif // DATABASE_H
