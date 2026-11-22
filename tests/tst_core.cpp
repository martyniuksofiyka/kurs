// Автоматичні тести предметної логіки та модуля доступу до даних.
// Кожен тест працює з окремою тимчасовою БД, робочі дані не змінюються.

#include "core/database.h"
#include "core/pricing.h"
#include "core/scheduling.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

using Pricing::Money;

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    // ---- розрахунок вартості ----
    void halfHoursRounding();
    void hallCostWeekend();
    void loyaltyDiscountBounds();
    void discountLimit();
    void lateFine();
    void rentalDays();

    // ---- алгоритми розкладу ----
    void overlapsTouching();
    void peakUsage();
    void findFreeSlot();
    void workingHours();

    // ---- база даних ----
    void init();
    void cleanup();
    void schemaAndForeignKeys();
    void uniquePhone();
    void foreignKeyRejected();
    void parametrizedQuery();
    void transactionRollback();
    void hallConflict();
    void propAvailability();
    void importClientsValid();
    void importClientsBroken();
    void normalizePhone();
    void reopenKeepsData();
    void backupCopy();
    void demoDataReset();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_dbFile;
    int addClient(const QString &name, const QString &phone);
};

static QDateTime dt(const QString &s)
{
    return QDateTime::fromString(s, "yyyy-MM-dd HH:mm");
}

// ---------------- розрахунок вартості ----------------

void TestCore::halfHoursRounding()
{
    QCOMPARE(Pricing::halfHours(dt("2026-11-23 10:00"), dt("2026-11-23 12:00")), 4);
    QCOMPARE(Pricing::halfHours(dt("2026-11-23 10:00"), dt("2026-11-23 10:10")), 1);   // неповні півгодини
    QCOMPARE(Pricing::halfHours(dt("2026-11-23 10:00"), dt("2026-11-23 10:00")), 0);
    QCOMPARE(Pricing::halfHours(dt("2026-11-23 12:00"), dt("2026-11-23 10:00")), 0);   // кінець раніше початку
}

void TestCore::hallCostWeekend()
{
    // 23.11.2026 - понеділок, 28.11.2026 - субота
    QCOMPARE(Pricing::hallCost(65000, dt("2026-11-23 10:00"), dt("2026-11-23 12:30")), Money(162500));
    QCOMPARE(Pricing::hallCost(65000, dt("2026-11-28 10:00"), dt("2026-11-28 12:30")), Money(195000));
}

void TestCore::loyaltyDiscountBounds()
{
    QCOMPARE(Pricing::loyaltyDiscount(0), 0);
    QCOMPARE(Pricing::loyaltyDiscount(2), 0);
    QCOMPARE(Pricing::loyaltyDiscount(3), 5);
    QCOMPARE(Pricing::loyaltyDiscount(9), 5);
    QCOMPARE(Pricing::loyaltyDiscount(10), 10);
}

void TestCore::discountLimit()
{
    QCOMPARE(Pricing::effectiveDiscount(0, 5), 5);
    QCOMPARE(Pricing::effectiveDiscount(15, 5), 15);
    QCOMPARE(Pricing::effectiveDiscount(50, 0), Pricing::MaxDiscount);
    // 1 грн 99 коп зі знижкою 5 % = 1,89 грн (округлення до копійки)
    QCOMPARE(Pricing::applyDiscount(199, 5), Money(189));
}

void TestCore::lateFine()
{
    const QDate planned(2026, 11, 20);
    QCOMPARE(Pricing::lateFine(30000, 1, planned, planned), Money(0));
    QCOMPARE(Pricing::lateFine(30000, 1, planned, planned.addDays(-1)), Money(0));
    // 2 дні × 2 шт × 300 грн × 150 %
    QCOMPARE(Pricing::lateFine(30000, 2, planned, planned.addDays(2)), Money(180000));
}

void TestCore::rentalDays()
{
    QCOMPARE(Pricing::rentalDays(QDate(2026, 11, 1), QDate(2026, 11, 1)), 1);
    QCOMPARE(Pricing::rentalDays(QDate(2026, 11, 1), QDate(2026, 11, 3)), 3);
    QCOMPARE(Pricing::rentalDays(QDate(2026, 11, 3), QDate(2026, 11, 1)), 0);
}

// ---------------- алгоритми розкладу ----------------

void TestCore::overlapsTouching()
{
    // бронювання, що йдуть одне за одним, не перетинаються
    QVERIFY(!Scheduling::overlaps(dt("2026-11-23 10:00"), dt("2026-11-23 12:00"),
                                  dt("2026-11-23 12:00"), dt("2026-11-23 13:00")));
    QVERIFY(Scheduling::overlaps(dt("2026-11-23 10:00"), dt("2026-11-23 12:00"),
                                 dt("2026-11-23 11:30"), dt("2026-11-23 13:00")));
    QVERIFY(Scheduling::overlaps(dt("2026-11-23 10:00"), dt("2026-11-23 18:00"),
                                 dt("2026-11-23 11:00"), dt("2026-11-23 12:00")));
}

void TestCore::peakUsage()
{
    QList<Scheduling::Interval> busy = {
        {dt("2026-11-23 10:00"), dt("2026-11-23 12:00"), 2},
        {dt("2026-11-23 11:00"), dt("2026-11-23 13:00"), 1},
        {dt("2026-11-23 12:00"), dt("2026-11-23 14:00"), 3},
    };
    // 11:00-12:00: 2+1 = 3; 12:00-13:00: 1+3 = 4
    QCOMPARE(Scheduling::peakUsage(busy, dt("2026-11-23 09:00"), dt("2026-11-23 15:00")), 4);
    QCOMPARE(Scheduling::peakUsage(busy, dt("2026-11-23 10:00"), dt("2026-11-23 11:00")), 2);
    QCOMPARE(Scheduling::peakUsage({}, dt("2026-11-23 10:00"), dt("2026-11-23 11:00")), 0);
    QCOMPARE(Scheduling::available(5, busy, dt("2026-11-23 09:00"), dt("2026-11-23 15:00")), 1);
    QCOMPARE(Scheduling::available(3, busy, dt("2026-11-23 09:00"), dt("2026-11-23 15:00")), 0);
}

void TestCore::findFreeSlot()
{
    const QDate day(2026, 11, 23);
    QList<Scheduling::Interval> busy = {
        {dt("2026-11-23 10:00"), dt("2026-11-23 13:00"), 1},
        {dt("2026-11-23 14:00"), dt("2026-11-23 18:00"), 1},
    };
    // бажано о 11:00 на 2 години: найближчий вільний варіант - 08:00 (о 13:00 лише година)
    QCOMPARE(Scheduling::findFreeSlot(busy, day, 120, QTime(8, 0), QTime(22, 0), QTime(11, 0)),
             dt("2026-11-23 08:00"));
    // година між бронюваннями
    QCOMPARE(Scheduling::findFreeSlot(busy, day, 60, QTime(8, 0), QTime(22, 0), QTime(13, 0)),
             dt("2026-11-23 13:00"));
    // увесь день зайнятий
    QList<Scheduling::Interval> full = {{dt("2026-11-23 08:00"), dt("2026-11-23 22:00"), 1}};
    QVERIFY(!Scheduling::findFreeSlot(full, day, 60, QTime(8, 0), QTime(22, 0), QTime(12, 0)).isValid());
}

void TestCore::workingHours()
{
    QVERIFY(Scheduling::withinWorkingHours(dt("2026-11-23 08:00"), dt("2026-11-23 22:00"), QTime(8, 0), QTime(22, 0)));
    QVERIFY(!Scheduling::withinWorkingHours(dt("2026-11-23 07:30"), dt("2026-11-23 09:00"), QTime(8, 0), QTime(22, 0)));
    QVERIFY(!Scheduling::withinWorkingHours(dt("2026-11-23 21:00"), dt("2026-11-23 22:30"), QTime(8, 0), QTime(22, 0)));
}

// ---------------- база даних ----------------

void TestCore::init()
{
    m_dir = new QTemporaryDir;
    m_dbFile = m_dir->filePath("test.db");
    QString error;
    QVERIFY2(Db::open(m_dbFile, &error), qPrintable(error));
    QVERIFY2(Db::createSchema(&error), qPrintable(error));
}

void TestCore::cleanup()
{
    Db::close();
    delete m_dir;
    m_dir = nullptr;
}

int TestCore::addClient(const QString &name, const QString &phone)
{
    if (!Db::exec("INSERT INTO clients (full_name, phone, created_at) VALUES (?, ?, '2026-11-01 10:00')", {name, phone}))
        return 0;
    return Db::scalar("SELECT last_insert_rowid()").toInt();
}

void TestCore::schemaAndForeignKeys()
{
    QCOMPARE(Db::scalar("PRAGMA foreign_keys").toInt(), 1);
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table'"
                        " AND name IN ('users','clients','halls','props','bookings','booking_props','rentals','rental_items')").toInt(), 8);
    // повторне створення схеми не має давати помилок
    QVERIFY(Db::createSchema());
}

void TestCore::uniquePhone()
{
    QVERIFY(addClient("Іван Тест", "+380501112233") > 0);
    QCOMPARE(addClient("Інший Клієнт", "+380501112233"), 0);
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM clients").toInt(), 1);
}

void TestCore::foreignKeyRejected()
{
    QVERIFY(Db::exec("INSERT INTO halls (name, price_per_hour) VALUES ('Зал', 50000)"));
    QString error;
    const bool ok = Db::exec("INSERT INTO bookings (client_id, hall_id, start_at, end_at, created_at)"
                             " VALUES (999, 1, '2026-11-23 10:00', '2026-11-23 11:00', '2026-11-01 10:00')", {}, &error);
    QVERIFY(!ok);
    QVERIFY(error.contains("FOREIGN KEY", Qt::CaseInsensitive));
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM bookings").toInt(), 0);
}

void TestCore::parametrizedQuery()
{
    const QString evil = "O'Brien'); DROP TABLE clients; --";
    QVERIFY(addClient(evil, "+380501112244") > 0);
    QCOMPARE(Db::scalar("SELECT full_name FROM clients WHERE phone = ?", {"+380501112244"}).toString(), evil);
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM sqlite_master WHERE name = 'clients'").toInt(), 1);
}

void TestCore::transactionRollback()
{
    const int client = addClient("Клієнт", "+380501112255");
    QVERIFY(Db::exec("INSERT INTO halls (name, price_per_hour) VALUES ('Зал', 50000)"));
    QVERIFY(Db::exec("INSERT INTO props (name, category, total_qty, price_per_hour, price_per_day) VALUES ('Фон', 'Фони', 2, 100, 1000)"));
    // тимчасовий тригер забороняє другу дію операції
    QVERIFY(Db::exec("CREATE TEMP TRIGGER reject_props BEFORE INSERT ON booking_props"
                     " BEGIN SELECT RAISE(ABORT, 'test rollback'); END"));

    QString error;
    const bool ok = Db::transaction([&](QString *err) {
        if (!Db::exec("INSERT INTO bookings (client_id, hall_id, start_at, end_at, created_at)"
                      " VALUES (?, 1, '2026-11-23 10:00', '2026-11-23 11:00', '2026-11-01 10:00')", {client}, err))
            return false;
        const int id = Db::scalar("SELECT last_insert_rowid()").toInt();
        return Db::exec("INSERT INTO booking_props (booking_id, prop_id, qty, price) VALUES (?, 1, 1, 100)", {id}, err);
    }, &error);

    QVERIFY(!ok);
    QVERIFY(error.contains("test rollback"));
    // перша зміна теж відкочена
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM bookings").toInt(), 0);
    QVERIFY(Db::exec("DROP TRIGGER reject_props"));
}

void TestCore::hallConflict()
{
    const int client = addClient("Клієнт", "+380501112266");
    QVERIFY(Db::exec("INSERT INTO halls (name, price_per_hour) VALUES ('Зал', 50000)"));
    QVERIFY(Db::exec("INSERT INTO bookings (client_id, hall_id, start_at, end_at, status, created_at)"
                     " VALUES (?, 1, '2026-11-23 10:00', '2026-11-23 12:00', 'confirmed', '2026-11-01 10:00')", {client}));
    const int id = Db::scalar("SELECT last_insert_rowid()").toInt();

    QCOMPARE(Db::hallConflict(1, dt("2026-11-23 11:00"), dt("2026-11-23 13:00")), id);
    QCOMPARE(Db::hallConflict(1, dt("2026-11-23 12:00"), dt("2026-11-23 13:00")), 0);   // впритул - можна
    QCOMPARE(Db::hallConflict(1, dt("2026-11-23 11:00"), dt("2026-11-23 13:00"), id), 0); // редагування себе
    QVERIFY(Db::exec("UPDATE bookings SET status = 'cancelled' WHERE id = ?", {id}));
    QCOMPARE(Db::hallConflict(1, dt("2026-11-23 11:00"), dt("2026-11-23 13:00")), 0);   // скасоване не заважає
}

void TestCore::propAvailability()
{
    const int client = addClient("Клієнт", "+380501112277");
    QVERIFY(Db::exec("INSERT INTO props (name, category, total_qty, price_per_hour, price_per_day) VALUES ('Софтбокс', 'Світло', 3, 100, 1000)"));
    QVERIFY(Db::exec("INSERT INTO rentals (client_id, start_date, end_date, status, created_at)"
                     " VALUES (?, '2030-01-10', '2030-01-12', 'active', '2026-11-01 10:00')", {client}));
    QVERIFY(Db::exec("INSERT INTO rental_items (rental_id, prop_id, qty, price_per_day) VALUES (1, 1, 2, 1000)"));

    QCOMPARE(Db::propAvailable(1, dt("2030-01-11 10:00"), dt("2030-01-11 12:00")), 1);
    QCOMPARE(Db::propAvailable(1, dt("2030-01-13 10:00"), dt("2030-01-13 12:00")), 3);
    QCOMPARE(Db::propAvailable(1, dt("2030-01-11 10:00"), dt("2030-01-11 12:00"), 0, 1), 3); // ігнорувати свою оренду
}

void TestCore::importClientsValid()
{
    const QString file = m_dir->filePath("clients.csv");
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("ПІБ;Телефон;E-mail\n"
            "Олена Тест;067 123 45 67;olena@test.ua\n"
            "Петро Тест;+380931234567;\n");
    f.close();

    int count = 0;
    QString error;
    QVERIFY2(Db::importClients(file, &count, &error), qPrintable(error));
    QCOMPARE(count, 2);
    QCOMPARE(Db::scalar("SELECT phone FROM clients WHERE full_name = 'Олена Тест'").toString(), QString("+380671234567"));
}

void TestCore::importClientsBroken()
{
    const QString file = m_dir->filePath("broken.csv");
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("ПІБ;Телефон\n"
            "Правильний Рядок;0671234567\n"
            "Помилковий Рядок;12345\n");
    f.close();

    int count = 0;
    QString error;
    QVERIFY(!Db::importClients(file, &count, &error));
    QVERIFY(error.contains("3"));            // номер рядка з помилкою
    QCOMPARE(count, 0);
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM clients").toInt(), 0);   // нічого не додано

    QVERIFY(!Db::importClients(m_dir->filePath("missing.csv"), &count, &error));
}

void TestCore::normalizePhone()
{
    QCOMPARE(Db::normalizePhone("+38 (067) 123-45-67"), QString("+380671234567"));
    QCOMPARE(Db::normalizePhone("0671234567"), QString("+380671234567"));
    QCOMPARE(Db::normalizePhone("380671234567"), QString("+380671234567"));
    QVERIFY(Db::normalizePhone("12345").isEmpty());
    QVERIFY(Db::normalizePhone("").isEmpty());
    QVERIFY(Db::normalizePhone("+48123456789").isEmpty());
}

void TestCore::reopenKeepsData()
{
    QVERIFY(addClient("Збережений Клієнт", "+380501112288") > 0);
    Db::close();
    QVERIFY(Db::open(m_dbFile));
    QCOMPARE(Db::scalar("SELECT full_name FROM clients WHERE phone = '+380501112288'").toString(),
             QString("Збережений Клієнт"));
}

void TestCore::backupCopy()
{
    QVERIFY(addClient("Клієнт Копії", "+380501112299") > 0);
    const QString copy = m_dir->filePath("backup.db");
    QString error;
    QVERIFY2(Db::backup(copy, &error), qPrintable(error));
    QVERIFY(QFile::exists(copy));

    // перевіряємо вміст копії через окреме з'єднання
    {
        QSqlDatabase b = QSqlDatabase::addDatabase("QSQLITE", "backup_check");
        b.setDatabaseName(copy);
        QVERIFY(b.open());
        QSqlQuery q("SELECT COUNT(*) FROM clients", b);
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }
    QSqlDatabase::removeDatabase("backup_check");
}

void TestCore::demoDataReset()
{
    QString error;
    QVERIFY2(Db::seedDemoData(&error), qPrintable(error));
    QVERIFY(Db::authenticate("admin", "admin123"));
    QVERIFY(!Db::authenticate("admin", "wrong"));
    QVERIFY(Db::scalar("SELECT COUNT(*) FROM bookings").toInt() > 50);

    // у навчальних даних не має бути накладок бронювань
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM bookings a JOIN bookings b ON a.hall_id = b.hall_id AND a.id < b.id"
                        " AND a.status <> 'cancelled' AND b.status <> 'cancelled'"
                        " AND a.start_at < b.end_at AND b.start_at < a.end_at").toInt(), 0);

    QVERIFY(addClient("Зайвий", "+380501110000") > 0);
    QVERIFY2(Db::resetDemoData(&error), qPrintable(error));
    QCOMPARE(Db::scalar("SELECT COUNT(*) FROM clients WHERE phone = '+380501110000'").toInt(), 0);
    QCOMPARE(Db::scalar("SELECT MIN(id) FROM clients").toInt(), 1);
}

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
