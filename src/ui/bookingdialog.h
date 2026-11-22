#ifndef BOOKINGDIALOG_H
#define BOOKINGDIALOG_H

#include <QDateTime>
#include <QDialog>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// Створення та редагування бронювання залу
class BookingDialog : public QDialog
{
    Q_OBJECT
public:
    // bookingId = 0 - нове бронювання
    explicit BookingDialog(int bookingId, QWidget *parent = nullptr);

    // попередньо обрати зал і час (з розкладу)
    void preset(int hallId, const QDateTime &start);

    int bookingId() const { return m_id; }

    // квитанція для друку
    static QString receiptHtml(int bookingId);

private slots:
    void recalc();
    void addClient();
    void findFreeTime();
    void save();
    void printReceipt();

private:
    void loadReferences();
    void loadBooking();
    void fillTimes();
    QDateTime startDateTime() const;
    QDateTime endDateTime() const;
    int currentClientId() const;
    int durationMinutes() const;

    int m_id = 0;
    bool m_loading = false;
    bool m_valid = false;

    QComboBox *m_client;
    QComboBox *m_hall;
    QDateEdit *m_date;
    QComboBox *m_start;
    QDoubleSpinBox *m_duration;
    QComboBox *m_status;
    QTableWidget *m_props;
    QSpinBox *m_discount;
    QDoubleSpinBox *m_prepaid;
    QPlainTextEdit *m_notes;

    QLabel *m_loyalty;
    QLabel *m_hallCost;
    QLabel *m_propsCost;
    QLabel *m_discountSum;
    QLabel *m_total;
    QLabel *m_toPay;
    QLabel *m_warning;
    QPushButton *m_findFree;
    QPushButton *m_saveBtn;

    qint64 m_totalValue = 0;
};

#endif // BOOKINGDIALOG_H
