#ifndef BOOKINGSPAGE_H
#define BOOKINGSPAGE_H

#include <QWidget>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QStandardItemModel;
class QTableView;
class TableProxy;

class BookingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit BookingsPage(QWidget *parent = nullptr);

public slots:
    void refresh();
    void addBooking();

private slots:
    void editSelected();
    void setStatus(const QString &status);
    void deleteSelected();
    void printReceipt();
    void exportCsv();

private:
    int selectedId() const;

    QLineEdit *m_search;
    QComboBox *m_hall;
    QComboBox *m_status;
    QDateEdit *m_from;
    QDateEdit *m_to;
    QTableView *m_view;
    QStandardItemModel *m_model;
    TableProxy *m_proxy;
    QLabel *m_footer;
};

#endif // BOOKINGSPAGE_H
