#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>
#include <QSvgRenderer>

namespace Theme {

void apply(QApplication &app)
{
    // Fusion однаково виглядає на Windows і macOS
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p = app.palette();
    p.setColor(QPalette::Window, Background);
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::AlternateBase, QColor("#f7f8fb"));
    p.setColor(QPalette::Highlight, Accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Text, QColor("#1f2433"));
    p.setColor(QPalette::WindowText, QColor("#1f2433"));
    p.setColor(QPalette::Button, Qt::white);
    p.setColor(QPalette::ButtonText, QColor("#1f2433"));
    p.setColor(QPalette::ToolTipBase, QColor("#1f2433"));
    p.setColor(QPalette::ToolTipText, Qt::white);
    app.setPalette(p);

    QFont f = app.font();
    f.setPointSize(f.pointSize() < 10 ? 10 : f.pointSize());
    app.setFont(f);

    QFile qss(":/style.qss");
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
}

QIcon icon(const QString &name, const QColor &color)
{
    QFile f(":/icons/" + name + ".svg");
    if (!f.open(QIODevice::ReadOnly))
        return QIcon();
    // у svg-файлах колір ліній заданий як currentColor
    QByteArray svg = f.readAll();
    svg.replace("currentColor", color.name().toLatin1());

    QSvgRenderer renderer(svg);
    QIcon result;
    for (int size : {16, 20, 24, 32, 48}) {
        QPixmap pm(size * 2, size * 2);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        renderer.render(&painter);
        painter.end();
        pm.setDevicePixelRatio(2.0);
        result.addPixmap(pm);
    }
    return result;
}

}
