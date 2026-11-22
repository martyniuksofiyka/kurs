#ifndef CLIENTSPAGE_H
#define CLIENTSPAGE_H

#include <QDialog>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStandardItemModel;
class QTableView;
class QTableWidget;
class TableProxy;

// Картка клієнта (додавання / редагування)
class ClientDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ClientDialog(int clientId, QWidget *parent = nullptr);
    int clientId() const { return m_id; }

    // відкриває форму нового клієнта, повертає id або 0
    static int createClient(QWidget *parent);

private slots:
    void save();

private:
    int m_id = 0;
    QLineEdit *m_name;
    QLineEdit *m_phone;
    QLineEdit *m_email;
    QPlainTextEdit *m_notes;
    QLabel *m_error;
};

class ClientsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ClientsPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void addClient();
    void editSelected();
    void deleteSelected();
    void importCsv();
    void exportCsv();
    void showHistory();

private:
    int selectedId() const;

    QLineEdit *m_search;
    QTableView *m_view;
    QStandardItemModel *m_model;
    TableProxy *m_proxy;
    QLabel *m_historyTitle;
    QTableWidget *m_history;
    QLabel *m_footer;
};

#endif // CLIENTSPAGE_H
