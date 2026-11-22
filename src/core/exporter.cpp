#include "exporter.h"
#include "session.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QTextDocument>
#include <QTextStream>

namespace Exporter {

static QString csvField(QString s)
{
    if (s.contains(';') || s.contains('"') || s.contains('\n')) {
        s.replace('"', "\"\"");
        s = '"' + s + '"';
    }
    return s;
}

bool saveCsv(const QString &fileName, const QAbstractItemModel *model, QString *error)
{
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    // BOM потрібен, щоб Excel правильно показав кирилицю
    f.write("\xEF\xBB\xBF");

    QStringList row;
    for (int c = 0; c < model->columnCount(); ++c)
        row << csvField(model->headerData(c, Qt::Horizontal).toString());
    f.write((row.join(';') + "\r\n").toUtf8());

    for (int r = 0; r < model->rowCount(); ++r) {
        row.clear();
        for (int c = 0; c < model->columnCount(); ++c)
            row << csvField(model->index(r, c).data().toString());
        f.write((row.join(';') + "\r\n").toUtf8());
    }
    return true;
}

QString modelToHtml(const QAbstractItemModel *model)
{
    QString html = "<table class='grid' width='100%' cellspacing='0' cellpadding='4'><tr>";
    for (int c = 0; c < model->columnCount(); ++c)
        html += "<th>" + model->headerData(c, Qt::Horizontal).toString().toHtmlEscaped() + "</th>";
    html += "</tr>";
    for (int r = 0; r < model->rowCount(); ++r) {
        html += "<tr>";
        for (int c = 0; c < model->columnCount(); ++c)
            html += "<td>" + model->index(r, c).data().toString().toHtmlEscaped() + "</td>";
        html += "</tr>";
    }
    return html + "</table>";
}

QString wrapHtml(const QString &title, const QString &body)
{
    const QString studio = Session::instance().studioName().toHtmlEscaped();
    return QString(
        "<html><head><meta charset='utf-8'><style>"
        "body { font-family: 'Arial'; font-size: 10pt; color: #222; }"
        "h1 { font-size: 16pt; margin: 0 0 4px 0; }"
        "h2 { font-size: 12pt; margin: 14px 0 6px 0; }"
        ".muted { color: #777; }"
        "table.grid { border-collapse: collapse; }"
        "table.grid th { background: #eef1f6; border: 1px solid #c9cfd8; text-align: left; }"
        "table.grid td { border: 1px solid #d9dde3; }"
        ".total { font-size: 12pt; font-weight: bold; }"
        "</style></head><body>"
        "<p class='muted'>%1</p><h1>%2</h1>%3"
        "<p class='muted'>Сформовано: %4</p>"
        "</body></html>")
        .arg(studio, title.toHtmlEscaped(), body,
             QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm"));
}

static void setupPrinter(QPrinter &printer)
{
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
}

bool savePdf(const QString &fileName, const QString &html, QString *error)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    setupPrinter(printer);

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);

    if (!QFile::exists(fileName)) {
        if (error)
            *error = QStringLiteral("Не вдалося створити файл");
        return false;
    }
    return true;
}

void printPreview(const QString &html, QWidget *parent)
{
    QPrinter printer(QPrinter::HighResolution);
    setupPrinter(printer);
    QPrintPreviewDialog dlg(&printer, parent);
    dlg.setWindowTitle(QStringLiteral("Попередній перегляд"));
    dlg.resize(900, 1000);
    QObject::connect(&dlg, &QPrintPreviewDialog::paintRequested, [&html](QPrinter *p) {
        QTextDocument doc;
        doc.setHtml(html);
        doc.print(p);
    });
    dlg.exec();
}

QString money(qint64 kopecks)
{
    QLocale ua(QLocale::Ukrainian, QLocale::Ukraine);
    return ua.toString(kopecks / 100.0, 'f', 2) + QStringLiteral(" грн");
}

}
