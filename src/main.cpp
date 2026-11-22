#include "core/database.h"
#include "ui/logindialog.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QTranslator>

#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("PhotoStudio");
    QApplication::setOrganizationName("PhotoStudio");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setApplicationDisplayName(QStringLiteral("PhotoStudio Manager"));

    // українські написи у стандартних діалогах Qt (кнопки, календар)
    QTranslator qtTranslator;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString trPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString trPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    if (qtTranslator.load("qtbase_uk", trPath))
        app.installTranslator(&qtTranslator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Система бронювання фотостудії та оренди реквізиту"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dbOption("db", QStringLiteral("Шлях до файла бази даних SQLite."), "file");
    QCommandLineOption resetOption("reset-demo", QStringLiteral("Очистити БД і заповнити навчальними даними."));
    parser.addOption(dbOption);
    parser.addOption(resetOption);
    parser.process(app);

    const QString dbPath = parser.isSet(dbOption) ? parser.value(dbOption) : Db::defaultPath();

    Theme::apply(app);

    QString error;
    if (!Db::open(dbPath, &error) || !Db::createSchema(&error)) {
        QMessageBox::critical(nullptr, QStringLiteral("Помилка бази даних"),
                              QStringLiteral("Не вдалося підготувати базу даних:\n%1\n\nФайл: %2").arg(error, dbPath));
        return 1;
    }

    if (parser.isSet(resetOption)) {
        if (!Db::resetDemoData(&error)) {
            std::cerr << "Reset failed: " << error.toStdString() << std::endl;
            return 1;
        }
    } else if (Db::isEmpty() && !Db::seedDemoData(&error)) {
        QMessageBox::critical(nullptr, QStringLiteral("Помилка"),
                              QStringLiteral("Не вдалося заповнити навчальні дані:\n%1").arg(error));
        return 1;
    }
    Db::log(QStringLiteral("Запуск програми"), dbPath);

    int code = 0;
    // цикл потрібен для пункту меню "Змінити користувача"
    for (;;) {
        LoginDialog login;
        if (login.exec() != QDialog::Accepted)
            break;
        Db::log(QStringLiteral("Вхід у систему"));

        MainWindow w;
        w.show();
        code = app.exec();
        if (!w.relogin())
            break;
    }

    Db::log(QStringLiteral("Завершення роботи"));
    Db::close();
    return code;
}
