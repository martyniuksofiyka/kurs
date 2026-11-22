#ifndef RENTALDIALOG_H
#define RENTALDIALOG_H

#include <QDialog>

class QComboBox;
class QDateEdit;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

// Оформлення оренди реквізиту на винос
class RentalDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RentalDialog(int rentalId, QWidget *parent = nullptr);

    // акт видачі для друку
    static QString actHtml(int rentalId);

private slots:
    void recalc();
    void addClient();
    void save();

private:
    void loadReferences();
    void loadRental();
    int currentClientId() const;

    int m_id = 0;
    bool m_loading = false;
    bool m_valid = false;
    bool m_readOnly = false;

    QComboBox *m_client;
    QDateEdit *m_start;
    QDateEdit *m_end;
    QTableWidget *m_items;
    QPlainTextEdit *m_notes;
    QLabel *m_days;
    QLabel *m_total;
    QLabel *m_deposit;
    QLabel *m_warning;
    QPushButton *m_saveBtn;
};

#endif // RENTALDIALOG_H
