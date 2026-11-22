#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QIcon>
#include <QString>

class QApplication;

namespace Theme {

const QColor Accent("#4f7cff");
const QColor Sidebar("#1f2433");
const QColor Background("#f4f6fa");
const QColor TextMuted("#7a8294");
const QColor Success("#2f9e6b");
const QColor Warning("#e0962b");
const QColor Danger("#d9534f");

void apply(QApplication &app);

// іконка з ресурсів, перефарбована в заданий колір
QIcon icon(const QString &name, const QColor &color = QColor("#5b6478"));

}

#endif // THEME_H
