#ifndef EXPORTER_H
#define EXPORTER_H

#include <QString>

class QAbstractItemModel;
class QWidget;

// Збереження даних у файли та друк документів
namespace Exporter {

// CSV з крапкою з комою - так Excel одразу розбиває на стовпці
bool saveCsv(const QString &fileName, const QAbstractItemModel *model, QString *error = nullptr);

// HTML-таблиця з даними моделі (для звітів)
QString modelToHtml(const QAbstractItemModel *model);

bool savePdf(const QString &fileName, const QString &html, QString *error = nullptr);

// попередній перегляд перед друком
void printPreview(const QString &html, QWidget *parent);

// спільна "шапка" документів зі стилями
QString wrapHtml(const QString &title, const QString &body);

// сума в копійках -> "1 250,00 грн"
QString money(qint64 kopecks);

}

#endif // EXPORTER_H
