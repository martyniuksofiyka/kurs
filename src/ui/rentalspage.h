#ifndef RENTALSPAGE_H
#define RENTALSPAGE_H

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QStandardItemModel;
class QTableView;
class TableProxy;

class RentalsPage : public QWidget
{
    Q_OBJECT
public:
    explicit RentalsPage(QWidget *parent = nullptr);

public slots:
    void refresh();
    void addRental();

private slots:
    void openSelected();
    void returnSelected();
    void cancelSelected();
    void exportCsv();

private:
    int selectedId() const;

    QLineEdit *m_search;
    QComboBox *m_filter;
    QTableView *m_view;
    QStandardItemModel *m_model;
    TableProxy *m_proxy;
    QLabel *m_footer;
};

#endif // RENTALSPAGE_H
