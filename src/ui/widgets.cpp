#include "widgets.h"
#include "theme.h"
#include "core/exporter.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardItem>
#include <QTableView>
#include <QVBoxLayout>

StatCard::StatCard(const QString &title, const QString &iconName, const QColor &color, QWidget *parent)
    : QFrame(parent)
{
    setObjectName("card");
    setMinimumHeight(104);

    auto *iconLabel = new QLabel;
    iconLabel->setFixedSize(42, 42);
    iconLabel->setAlignment(Qt::AlignCenter);
    QColor bg = color;
    bg.setAlpha(30);
    iconLabel->setStyleSheet(QString("background: rgba(%1,%2,%3,%4); border-radius: 10px;")
                                 .arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(bg.alpha()));
    iconLabel->setPixmap(Theme::icon(iconName, color).pixmap(22, 22));

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("cardTitle");
    m_value = new QLabel("—");
    m_value->setObjectName("cardValue");
    m_hint = new QLabel;
    m_hint->setObjectName("cardHint");

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(titleLabel);
    text->addWidget(m_value);
    text->addWidget(m_hint);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->addLayout(text, 1);
    lay->addWidget(iconLabel, 0, Qt::AlignTop);
}

void StatCard::setValue(const QString &value, const QString &hint)
{
    m_value->setText(value);
    m_hint->setText(hint);
    m_hint->setVisible(!hint.isEmpty());
}

void StatusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QString text = opt.text;
    opt.text.clear();
    // фон рядка (виділення) малює стандартний стиль, але вже без тексту
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    if (text.isEmpty())
        return;

    QColor c = Ui::statusColorByText(text);
    QFont f = opt.font;
    f.setPointSizeF(f.pointSizeF() * 0.92);
    f.setBold(true);
    QFontMetrics fm(f);
    const int w = fm.horizontalAdvance(text) + 18;
    const int h = fm.height() + 6;
    QRectF r(opt.rect.left() + 6, opt.rect.center().y() - h / 2.0, w, h);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QColor bg = c;
    bg.setAlpha(36);
    QPainterPath path;
    path.addRoundedRect(r, h / 2.0, h / 2.0);
    painter->fillPath(path, bg);
    painter->setPen(c.darker(115));
    painter->setFont(f);
    painter->drawText(r, Qt::AlignCenter, text);
    painter->restore();
}

QSize StatusDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    return QSize(s.width() + 30, qMax(s.height(), 30));
}

bool TableProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const QVariant l = left.data(SortRole);
    const QVariant r = right.data(SortRole);
    if (l.isValid() && r.isValid()) {
        bool okL = false, okR = false;
        const double dl = l.toDouble(&okL);
        const double dr = r.toDouble(&okR);
        if (okL && okR)
            return dl < dr;
        return l.toString() < r.toString();
    }
    return QString::localeAwareCompare(left.data().toString(), right.data().toString()) < 0;
}

namespace Ui {

QStandardItem *cell(const QString &text, const QVariant &sortValue)
{
    auto *item = new QStandardItem(text);
    if (sortValue.isValid())
        item->setData(sortValue, SortRole);
    return item;
}

QStandardItem *moneyCell(qint64 kopecks)
{
    QStandardItem *item = cell(Exporter::money(kopecks), kopecks);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

QString bookingStatusText(const QString &code)
{
    if (code == "new") return QStringLiteral("Нове");
    if (code == "confirmed") return QStringLiteral("Підтверджено");
    if (code == "completed") return QStringLiteral("Завершено");
    if (code == "cancelled") return QStringLiteral("Скасовано");
    return code;
}

QColor bookingStatusColor(const QString &code)
{
    if (code == "new") return Theme::Warning;
    if (code == "confirmed") return Theme::Accent;
    if (code == "completed") return Theme::Success;
    return QColor("#9aa1ae");
}

QString rentalStatusText(const QString &code, bool overdue)
{
    if (code == "active") return overdue ? QStringLiteral("Прострочено") : QStringLiteral("Видано");
    if (code == "returned") return QStringLiteral("Повернуто");
    if (code == "cancelled") return QStringLiteral("Скасовано");
    return code;
}

QColor rentalStatusColor(const QString &code, bool overdue)
{
    if (code == "active") return overdue ? Theme::Danger : Theme::Accent;
    if (code == "returned") return Theme::Success;
    return QColor("#9aa1ae");
}

QColor statusColorByText(const QString &text)
{
    if (text == QStringLiteral("Нове")) return Theme::Warning;
    if (text == QStringLiteral("Підтверджено") || text == QStringLiteral("Видано")) return Theme::Accent;
    if (text == QStringLiteral("Завершено") || text == QStringLiteral("Повернуто")
        || text == QStringLiteral("Активний")) return Theme::Success;
    if (text == QStringLiteral("Прострочено")) return Theme::Danger;
    return QColor("#8a92a0");
}

QWidget *pageHeader(const QString &title, const QString &subtitle, QWidget *right)
{
    auto *w = new QWidget;
    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("pageTitle");
    auto *sub = new QLabel(subtitle);
    sub->setObjectName("pageSubtitle");

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(titleLabel);
    text->addWidget(sub);

    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 6);
    lay->addLayout(text, 1);
    if (right)
        lay->addWidget(right, 0, Qt::AlignBottom);
    return w;
}

void setupTable(QTableView *view)
{
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setAlternatingRowColors(true);
    view->setShowGrid(false);
    view->setSortingEnabled(true);
    // до першого кліку по заголовку зберігаємо порядок, заданий у SQL-запиті
    view->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    view->setWordWrap(false);
    view->verticalHeader()->hide();
    view->verticalHeader()->setDefaultSectionSize(36);
    view->horizontalHeader()->setHighlightSections(false);
    view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    view->horizontalHeader()->setStretchLastSection(true);
}

bool confirm(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.button(QMessageBox::Yes)->setText(QStringLiteral("Так"));
    box.button(QMessageBox::No)->setText(QStringLiteral("Ні"));
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}

}
