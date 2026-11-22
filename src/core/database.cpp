#include "database.h"
#include "pricing.h"
#include "session.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>

namespace Db {

const char *DateTimeFmt = "yyyy-MM-dd HH:mm";
const char *DateFmt = "yyyy-MM-dd";

static QString g_lastError;
static QString g_path;

QString toDb(const QDateTime &dt) { return dt.toString(DateTimeFmt); }
QString toDb(const QDate &d) { return d.toString(DateFmt); }

QDateTime dtFromDb(const QVariant &v)
{
    return QDateTime::fromString(v.toString(), DateTimeFmt);
}

QDate dateFromDb(const QVariant &v)
{
    return QDate::fromString(v.toString().left(10), DateFmt);
}

QString lastError() { return g_lastError; }

QString defaultPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath("photostudio.db");
}

QString path() { return g_path; }

void log(const QString &action, const QString &details)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QFile f(QDir(dir).filePath("photostudio.log"));
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " | "
        << Session::instance().login << " | " << action;
    if (!details.isEmpty())
        out << " | " << details;
    out << '\n';
}

static bool fail(const QString &message, QString *error)
{
    g_lastError = message;
    if (error)
        *error = message;
    log("ПОМИЛКА", message);
    return false;
}

bool open(const QString &fileName, QString *error)
{
    const QFileInfo info(fileName);
    if (!QDir().mkpath(info.absolutePath()))
        return fail(QStringLiteral("Не вдалося створити каталог %1").arg(info.absolutePath()), error);
    if (info.exists() && !info.isWritable())
        return fail(QStringLiteral("Немає прав на запис у файл %1").arg(fileName), error);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(fileName);
    if (!db.open())
        return fail(QStringLiteral("Не вдалося відкрити БД: %1").arg(db.lastError().text()), error);
    g_path = info.absoluteFilePath();

    // SQLite за замовчуванням не перевіряє зовнішні ключі, вмикаємо явно
    // і перевіряємо, що режим справді увімкнено
    QSqlQuery q(db);
    if (!q.exec("PRAGMA foreign_keys = ON") || !q.exec("PRAGMA foreign_keys") || !q.next()
        || q.value(0).toInt() != 1)
        return fail(QStringLiteral("Не вдалося увімкнути контроль зовнішніх ключів"), error);
    return true;
}

void close()
{
    // з'єднання видаляємо лише після того, як знищено всі запити й моделі
    const QString name = QSqlDatabase::database().connectionName();
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(name);
}

bool exec(const QString &sql, const QVariantList &args, QString *error, int *rowsAffected)
{
    QSqlQuery q;
    if (!q.prepare(sql))
        return fail(q.lastError().text(), error);
    for (const QVariant &a : args)
        q.addBindValue(a);
    if (!q.exec())
        return fail(q.lastError().text(), error);
    if (rowsAffected)
        *rowsAffected = q.numRowsAffected();
    return true;
}

QVariant scalar(const QString &sql, const QVariantList &args)
{
    QSqlQuery q;
    q.prepare(sql);
    for (const QVariant &a : args)
        q.addBindValue(a);
    if (q.exec() && q.next())
        return q.value(0);
    return QVariant();
}

bool transaction(const std::function<bool(QString *)> &action, QString *error)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction())
        return fail(QStringLiteral("Не вдалося почати транзакцію: %1").arg(db.lastError().text()), error);

    QString err;
    if (action(&err)) {
        if (db.commit())
            return true;
        err = QStringLiteral("Помилка підтвердження транзакції: %1").arg(db.lastError().text());
    }
    if (!db.rollback())
        err += QStringLiteral("\nВідкат також завершився помилкою: %1").arg(db.lastError().text());
    return fail(err, error);
}

bool createSchema(QString *error)
{
    QFile f(":/db/schema.sql");
    if (!f.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Не знайдено файл схеми БД"), error);
    QString script = QString::fromUtf8(f.readAll());

    // прибираємо коментарі й розбиваємо скрипт на окремі команди
    script.remove(QRegularExpression("--[^\\n]*"));
    const QStringList statements = script.split(';');

    return transaction([&](QString *err) {
        for (const QString &s : statements) {
            if (s.trimmed().isEmpty())
                continue;
            if (!exec(s, {}, err))
                return false;
        }
        return true;
    }, error);
}

bool isEmpty()
{
    return scalar("SELECT COUNT(*) FROM users").toInt() == 0;
}

// ---------- користувачі ----------

QString makeSalt()
{
    QByteArray bytes;
    for (int i = 0; i < 16; ++i)
        bytes.append(char(QRandomGenerator::global()->bounded(256)));
    return QString::fromLatin1(bytes.toHex());
}

QString hashPassword(const QString &password, const QString &salt)
{
    const QByteArray data = (salt + password).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool authenticate(const QString &login, const QString &password)
{
    QSqlQuery q;
    q.prepare("SELECT id, full_name, role, pass_hash, salt FROM users WHERE login = ? AND active = 1");
    q.addBindValue(login.trimmed());
    if (!q.exec() || !q.next())
        return false;
    if (hashPassword(password, q.value(4).toString()) != q.value(3).toString())
        return false;

    Session &s = Session::instance();
    s.userId = q.value(0).toInt();
    s.login = login.trimmed();
    s.fullName = q.value(1).toString();
    s.role = q.value(2).toString();
    return true;
}

bool addUser(const QString &login, const QString &fullName, const QString &role,
             const QString &password, QString *error)
{
    const QString salt = makeSalt();
    return exec("INSERT INTO users (login, full_name, role, pass_hash, salt) VALUES (?, ?, ?, ?, ?)",
                {login.trimmed(), fullName.trimmed(), role, hashPassword(password, salt), salt}, error);
}

bool setPassword(int userId, const QString &password)
{
    const QString salt = makeSalt();
    int rows = 0;
    return exec("UPDATE users SET pass_hash = ?, salt = ? WHERE id = ?",
                {hashPassword(password, salt), salt, userId}, nullptr, &rows) && rows == 1;
}

// ---------- зайнятість ----------

int hallConflict(int hallId, const QDateTime &from, const QDateTime &to, int ignoreBookingId)
{
    // дати мають однаковий формат ISO, тому їх можна порівнювати як рядки
    return scalar("SELECT id FROM bookings WHERE hall_id = ? AND status <> 'cancelled'"
                  " AND id <> ? AND start_at < ? AND end_at > ? ORDER BY start_at LIMIT 1",
                  {hallId, ignoreBookingId, toDb(to), toDb(from)}).toInt();
}

QList<Scheduling::Interval> hallBusy(int hallId, const QDate &date, int ignoreBookingId)
{
    QList<Scheduling::Interval> list;
    QSqlQuery q;
    q.prepare("SELECT start_at, end_at FROM bookings WHERE hall_id = ? AND status <> 'cancelled'"
              " AND id <> ? AND substr(start_at, 1, 10) = ?");
    q.addBindValue(hallId);
    q.addBindValue(ignoreBookingId);
    q.addBindValue(toDb(date));
    if (q.exec()) {
        while (q.next())
            list.append({dtFromDb(q.value(0)), dtFromDb(q.value(1)), 1});
    }
    return list;
}

QList<Scheduling::Interval> propBusy(int propId, int ignoreBookingId, int ignoreRentalId)
{
    QList<Scheduling::Interval> list;

    QSqlQuery q;
    q.prepare("SELECT b.start_at, b.end_at, bp.qty FROM booking_props bp"
              " JOIN bookings b ON b.id = bp.booking_id"
              " WHERE bp.prop_id = ? AND b.status <> 'cancelled' AND b.id <> ?");
    q.addBindValue(propId);
    q.addBindValue(ignoreBookingId);
    if (q.exec()) {
        while (q.next())
            list.append({dtFromDb(q.value(0)), dtFromDb(q.value(1)), q.value(2).toInt()});
    }

    // реквізит в оренді зайнятий з початку першого дня до кінця останнього,
    // а якщо його ще не повернули - щонайменше до поточного моменту
    q.prepare("SELECT r.start_date, r.end_date, r.returned_at, r.status, ri.qty FROM rental_items ri"
              " JOIN rentals r ON r.id = ri.rental_id"
              " WHERE ri.prop_id = ? AND r.status <> 'cancelled' AND r.id <> ?");
    q.addBindValue(propId);
    q.addBindValue(ignoreRentalId);
    if (q.exec()) {
        const QDateTime now = QDateTime::currentDateTime();
        while (q.next()) {
            const QDateTime from(dateFromDb(q.value(0)), QTime(0, 0));
            QDateTime to(dateFromDb(q.value(1)).addDays(1), QTime(0, 0));
            if (q.value(3).toString() == "returned") {
                const QDateTime ret = dtFromDb(q.value(2));
                if (ret.isValid())
                    to = ret;
            } else if (to < now) {
                to = now;
            }
            list.append({from, to, q.value(4).toInt()});
        }
    }
    return list;
}

int propAvailable(int propId, const QDateTime &from, const QDateTime &to,
                  int ignoreBookingId, int ignoreRentalId)
{
    const int total = scalar("SELECT total_qty FROM props WHERE id = ?", {propId}).toInt();
    return Scheduling::available(total, propBusy(propId, ignoreBookingId, ignoreRentalId), from, to);
}

int completedVisits(int clientId)
{
    return scalar("SELECT COUNT(*) FROM bookings WHERE client_id = ? AND status = 'completed'",
                  {clientId}).toInt();
}

// ---------- клієнти ----------

QString normalizePhone(const QString &phone)
{
    QString digits;
    for (const QChar c : phone) {
        if (c.isDigit())
            digits += c;
    }
    // допускаємо введення 0XXXXXXXXX, 380XXXXXXXXX або +380XXXXXXXXX
    if (digits.length() == 10 && digits.startsWith('0'))
        digits = "38" + digits;
    if (digits.length() != 12 || !digits.startsWith("380"))
        return QString();
    return '+' + digits;
}

bool importClients(const QString &fileName, int *imported, QString *error)
{
    *imported = 0;
    QFile f(fileName);
    if (!f.exists())
        return fail(QStringLiteral("Файл не знайдено: %1").arg(fileName), error);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return fail(QStringLiteral("Не вдалося відкрити файл: %1").arg(f.errorString()), error);

    const QString text = QString::fromUtf8(f.readAll());
    QStringList lines = text.split('\n');
    if (lines.isEmpty() || !lines.first().contains(';'))
        return fail(QStringLiteral("Невідомий формат файла. Очікується CSV з роздільником ';'"), error);
    lines.removeFirst();   // рядок заголовків

    // спочатку перевіряємо всі рядки, і лише потім пишемо в БД
    struct Row { QString name, phone, email, notes; };
    QList<Row> rows;
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.startsWith(QChar(0xFEFF)))
            line.remove(0, 1);
        if (line.isEmpty())
            continue;
        const QStringList parts = line.split(';');
        const int lineNo = i + 2;
        if (parts.size() < 2)
            return fail(QStringLiteral("Рядок %1: потрібні щонайменше ПІБ і телефон").arg(lineNo), error);
        // формат: ПІБ;телефон;e-mail (решта стовпців, якщо є, ігнорується)
        Row r{parts[0].trimmed(), normalizePhone(parts[1]), parts.value(2).trimmed(), QString()};
        if (r.name.length() < 2 || r.name.length() > 100)
            return fail(QStringLiteral("Рядок %1: некоректне ПІБ").arg(lineNo), error);
        if (r.phone.isEmpty())
            return fail(QStringLiteral("Рядок %1: некоректний номер телефону «%2»").arg(lineNo).arg(parts[1]), error);
        rows.append(r);
    }
    if (rows.isEmpty())
        return fail(QStringLiteral("У файлі немає записів для імпорту"), error);

    const QString now = toDb(QDateTime::currentDateTime());
    const bool ok = transaction([&](QString *err) {
        for (const Row &r : rows) {
            if (scalar("SELECT COUNT(*) FROM clients WHERE phone = ?", {r.phone}).toInt() > 0) {
                *err = QStringLiteral("Клієнт з номером %1 уже є в базі").arg(r.phone);
                return false;
            }
            if (!exec("INSERT INTO clients (full_name, phone, email, notes, created_at) VALUES (?, ?, ?, ?, ?)",
                      {r.name, r.phone, r.email, r.notes, now}, err))
                return false;
        }
        return true;
    }, error);
    if (ok) {
        *imported = rows.size();
        log("Імпорт клієнтів", QStringLiteral("%1 записів з %2").arg(rows.size()).arg(QFileInfo(fileName).fileName()));
    }
    return ok;
}

bool backup(const QString &fileName, QString *error)
{
    if (QFile::exists(fileName) && !QFile::remove(fileName))
        return fail(QStringLiteral("Не вдалося перезаписати файл %1").arg(fileName), error);
    // VACUUM INTO створює узгоджену копію навіть під час роботи програми
    if (!exec("VACUUM INTO ?", {fileName}, error))
        return false;
    log("Резервна копія", fileName);
    return true;
}

// ---------- навчальні дані ----------

bool seedDemoData(QString *error)
{
    using Pricing::Money;

    return transaction([](QString *err) -> bool {
        if (!addUser("admin", "Адміністратор системи", "admin", "admin123", err)
            || !addUser("manager", "Оксана Мельник", "manager", "manager123", err))
            return false;

        struct Hall { const char *name; double area; Money price; const char *descr; const char *color; };
        const Hall halls[] = {
            {"Білий циклорама", 85, 65000, "Безшовна біла циклорама 6×4 м, імпульсне світло, підходить для каталожної та портретної зйомки", "#4f7cff"},
            {"Лофт", 70, 55000, "Цегляні стіни, великі вікна з денним світлом, шкіряний диван і дерев'яна підлога", "#e3893c"},
            {"Затишний дім", 60, 50000, "Інтер'єр вітальні та спальні, ліжко, кухонний острів, багато текстилю", "#3fae7a"},
            {"Dark Room", 45, 45000, "Чорні стіни, кольорові RGB-прилади, генератор диму для креативних зйомок", "#9a5bd6"},
        };
        for (const Hall &h : halls) {
            if (!exec("INSERT INTO halls (name, area, price_per_hour, description, color) VALUES (?, ?, ?, ?, ?)",
                      {QString::fromUtf8(h.name), h.area, h.price, QString::fromUtf8(h.descr),
                       QString::fromLatin1(h.color)}, err))
                return false;
        }

        struct Prop { const char *name; const char *cat; int qty; Money ph; Money pd; Money dep; };
        const Prop props[] = {
            {"Фон паперовий білий, 2,7 м", "Фони", 4, 4000, 25000, 50000},
            {"Фон паперовий чорний, 2,7 м", "Фони", 3, 4000, 25000, 50000},
            {"Фон тканинний бежевий", "Фони", 2, 3000, 20000, 40000},
            {"Софтбокс 90×120 см", "Світло", 6, 6000, 35000, 150000},
            {"Октобокс 120 см", "Світло", 3, 7000, 40000, 200000},
            {"Кільцева лампа 18\"", "Світло", 4, 3500, 20000, 80000},
            {"Генератор диму", "Ефекти", 2, 12000, 60000, 300000},
            {"Генератор мильних бульбашок", "Ефекти", 2, 6000, 30000, 100000},
            {"Арка з живих квітів", "Декор", 1, 25000, 150000, 500000},
            {"Вінтажне крісло", "Меблі", 2, 5000, 30000, 200000},
            {"Пуф оксамитовий", "Меблі", 3, 3000, 15000, 80000},
            {"Фатин для фотосесій, 5 м", "Текстиль", 6, 2000, 10000, 30000},
            {"Повітряні кульки, набір 30 шт", "Декор", 10, 0, 45000, 0},
            {"Відбивач 5 в 1, 110 см", "Світло", 5, 2500, 12000, 60000},
        };
        for (const Prop &p : props) {
            if (!exec("INSERT INTO props (name, category, total_qty, price_per_hour, price_per_day, deposit, condition)"
                      " VALUES (?, ?, ?, ?, ?, ?, 'Добрий')",
                      {QString::fromUtf8(p.name), QString::fromUtf8(p.cat), p.qty, p.ph, p.pd, p.dep}, err))
                return false;
        }

        const char *clients[][3] = {
            {"Ірина Ковальчук", "+380671234501", "irynka.k@gmail.com"},
            {"Андрій Бойко", "+380502345602", "boyko.andrii@ukr.net"},
            {"Марта Гнатюк", "+380973456703", "marta.photo@gmail.com"},
            {"Олег Савчук", "+380634567804", ""},
            {"Софія Дмитрук", "+380955678905", "sofiadm@gmail.com"},
            {"Тарас Мороз", "+380686789006", "taras.moroz@i.ua"},
            {"Катерина Лисенко", "+380507890107", "kate.lysenko@gmail.com"},
            {"Віталій Кравець", "+380968901208", ""},
            {"Юлія Петрук", "+380679012309", "yulia.petruk@gmail.com"},
            {"Дмитро Шевчук", "+380630123410", "d.shevchuk@ukr.net"},
            {"Наталія Онищук", "+380991234511", "natali.on@gmail.com"},
            {"Студія «Pixel Wedding»", "+380671112233", "hello@pixelwedding.com.ua"},
            {"Роман Ткачук", "+380502223344", "r.tkachuk@gmail.com"},
            {"Анастасія Біла", "+380673334455", "nastia.bila@gmail.com"},
            {"Максим Гуменюк", "+380934445566", ""},
            {"Вікторія Сидорук", "+380665556677", "vika.sydoruk@ukr.net"},
            {"Богдан Литвин", "+380976667788", "bohdan.lytvyn@gmail.com"},
            {"Христина Мельничук", "+380687778899", "khrystyna.m@gmail.com"},
            {"Ольга Паламарчук", "+380508889900", ""},
            {"Назар Іванишин", "+380639990011", "nazar.iv@gmail.com"},
            {"Агенція «Lumen Models»", "+380671230000", "booking@lumenmodels.com.ua"},
            {"Світлана Федорів", "+380962345678", "svitlana.fed@i.ua"},
        };
        const int clientCount = int(sizeof(clients) / sizeof(clients[0]));
        const QString created = toDb(QDateTime::currentDateTime().addMonths(-3));
        for (auto &c : clients) {
            if (!exec("INSERT INTO clients (full_name, phone, email, created_at) VALUES (?, ?, ?, ?)",
                      {QString::fromUtf8(c[0]), QString::fromLatin1(c[1]), QString::fromLatin1(c[2]), created}, err))
                return false;
        }

        // бронювання за останні 5 тижнів і на 2 тижні вперед;
        // фіксоване зерно дає однаковий набір даних при кожному відновленні
        QRandomGenerator rnd(2026);
        const QDate today = QDate::currentDate();
        const int hallCount = int(sizeof(halls) / sizeof(halls[0]));
        const int propCount = int(sizeof(props) / sizeof(props[0]));

        for (int day = -35; day <= 14; ++day) {
            const QDate date = today.addDays(day);
            for (int hall = 1; hall <= hallCount; ++hall) {
                int hour = 9 + int(rnd.bounded(3));
                const int perDay = int(rnd.bounded(Pricing::isWeekend(date) ? 4 : 3));
                for (int k = 0; k < perDay && hour < 20; ++k) {
                    const int duration = 1 + int(rnd.bounded(3));
                    const QDateTime from(date, QTime(hour, rnd.bounded(2) ? 30 : 0));
                    const QDateTime to = from.addSecs(duration * 3600);
                    if (to.time() > QTime(22, 0) || to.date() != date)
                        break;
                    hour = to.time().hour() + 1 + int(rnd.bounded(2));

                    QString status;
                    if (day < 0)
                        status = rnd.bounded(10) == 0 ? "cancelled" : "completed";
                    else if (day == 0)
                        status = "confirmed";
                    else
                        status = rnd.bounded(3) == 0 ? "new" : "confirmed";

                    const int clientId = 1 + int(rnd.bounded(clientCount));
                    const Money hallCost = Pricing::hallCost(halls[hall - 1].price, from, to);

                    int propId = 0;
                    Money propsCost = 0;
                    if (rnd.bounded(3) == 0) {
                        propId = 1 + int(rnd.bounded(propCount));
                        if (props[propId - 1].ph > 0)
                            propsCost = Pricing::propCostHourly(props[propId - 1].ph, 1, Pricing::halfHours(from, to));
                        else
                            propId = 0;
                    }
                    const int discount = rnd.bounded(6) == 0 ? 5 : 0;
                    const Money total = Pricing::applyDiscount(hallCost + propsCost, discount);
                    Money prepaid = 0;
                    if (status == "completed")
                        prepaid = total;
                    else if (status == "confirmed")
                        prepaid = Pricing::mulDiv(total, 30, 100);

                    if (!exec("INSERT INTO bookings (client_id, hall_id, start_at, end_at, status, hall_cost, props_cost,"
                              " discount, total, prepaid, created_by, created_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
                              {clientId, hall, toDb(from), toDb(to), status, hallCost, propsCost,
                               discount, total, prepaid, 2, toDb(from.addDays(-3))}, err))
                        return false;
                    if (propId > 0) {
                        const int bookingId = scalar("SELECT last_insert_rowid()").toInt();
                        if (!exec("INSERT INTO booking_props (booking_id, prop_id, qty, price) VALUES (?, ?, 1, ?)",
                                  {bookingId, propId, props[propId - 1].ph}, err))
                            return false;
                    }
                }
            }
        }

        // оренда на винос: повернута, поточна, прострочена й запланована
        struct Rent { int client; int startOffset; int days; int prop; int qty; const char *status; };
        const Rent rents[] = {
            {3, -20, 2, 4, 2, "returned"},
            {6, -14, 3, 7, 1, "returned"},
            {12, -9, 4, 9, 1, "returned"},
            {1, -6, 1, 13, 3, "returned"},
            {9, -3, 5, 5, 1, "active"},
            {12, -1, 3, 4, 3, "active"},
            {4, -7, 3, 10, 1, "active"},     // мав повернути 3 дні тому
            {7, 2, 2, 12, 4, "active"},
        };
        for (const Rent &r : rents) {
            const QDate start = today.addDays(r.startOffset);
            const QDate end = start.addDays(r.days - 1);
            const Prop &p = props[r.prop - 1];
            const QString status = QString::fromLatin1(r.status);
            QVariant returned;
            if (status == "returned")
                returned = toDb(QDateTime(end, QTime(18, 0)));
            if (!exec("INSERT INTO rentals (client_id, start_date, end_date, returned_at, status, total, deposit,"
                      " created_by, created_at) VALUES (?,?,?,?,?,?,?,?,?)",
                      {r.client, toDb(start), toDb(end), returned, status,
                       Pricing::propCostDaily(p.pd, r.qty, r.days), p.dep * r.qty, 2,
                       toDb(QDateTime(start, QTime(10, 0)))}, err))
                return false;
            const int rentalId = scalar("SELECT last_insert_rowid()").toInt();
            if (!exec("INSERT INTO rental_items (rental_id, prop_id, qty, price_per_day) VALUES (?, ?, ?, ?)",
                      {rentalId, r.prop, r.qty, p.pd}, err))
                return false;
        }
        return true;
    }, error);
}

bool resetDemoData(QString *error)
{
    const QStringList tables = {"booking_props", "rental_items", "bookings", "rentals",
                                "clients", "props", "halls", "users"};
    const bool cleared = transaction([&](QString *err) {
        for (const QString &t : tables) {
            if (!exec("DELETE FROM " + t, {}, err))
                return false;
        }
        // скидаємо лічильники AUTOINCREMENT, щоб id починались з 1
        return exec("DELETE FROM sqlite_sequence", {}, err);
    }, error);
    if (!cleared)
        return false;
    if (!seedDemoData(error))
        return false;
    log("Відновлення навчальних даних");
    return true;
}

}
