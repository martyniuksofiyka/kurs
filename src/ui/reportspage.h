#ifndef REPORTSPAGE_H
#define REPORTSPAGE_H

#include <QWidget>

class BarChart;
class QComboBox;
class QDateEdit;
class QLabel;
class QStandardItemModel;
class QTableView;

class ReportsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReportsPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void savePdf();
    void saveCsv();
    void print();

private:
    void reportHalls();
    void reportProps();
    void reportClients();
    void reportDays();
    void reportWeekdays();
    QString reportHtml() const;

    QComboBox *m_type;
    QDateEdit *m_from;
    QDateEdit *m_to;
    BarChart *m_chart;
    QTableView *m_view;
    QStandardItemModel *m_model;
    QLabel *m_summary;
};

#endif // REPORTSPAGE_H
