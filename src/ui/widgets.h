#ifndef WIDGETS_H
#define WIDGETS_H

#include <QColor>
#include <QFrame>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

class QLabel;
class QTableView;
class QWidget;

// Невеликі віджети та функції, які використовуються на кількох сторінках

// картка з показником на головній сторінці
class StatCard : public QFrame
{
    Q_OBJECT
public:
    StatCard(const QString &title, const QString &iconName, const QColor &color, QWidget *parent = nullptr);
    void setValue(const QString &value, const QString &hint = QString());

private:
    QLabel *m_value;
    QLabel *m_hint;
};

// відображення статусу у вигляді кольорової "пігулки" в таблиці
class StatusDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

// роль, у якій зберігається значення для сортування (число, дата)
constexpr int SortRole = Qt::UserRole + 1;
// роль з ідентифікатором запису (у першому стовпці)
constexpr int IdRole = Qt::UserRole;

// сортує за SortRole, якщо воно задане, і шукає по всіх стовпцях
class TableProxy : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

class QStandardItem;

namespace Ui {

using ::IdRole;
using ::SortRole;

QStandardItem *cell(const QString &text, const QVariant &sortValue = QVariant());
QStandardItem *moneyCell(qint64 kopecks);

QString bookingStatusText(const QString &code);
QColor bookingStatusColor(const QString &code);
QString rentalStatusText(const QString &code, bool overdue = false);
QColor rentalStatusColor(const QString &code, bool overdue = false);
QColor statusColorByText(const QString &text);

// заголовок сторінки з підзаголовком
QWidget *pageHeader(const QString &title, const QString &subtitle, QWidget *right = nullptr);

void setupTable(QTableView *view);

bool confirm(QWidget *parent, const QString &title, const QString &text);

}

#endif // WIDGETS_H
