#ifndef USERSPAGE_H
#define USERSPAGE_H

#include <QWidget>

class QStandardItemModel;
class QTableView;

// Керування обліковими записами (доступно лише адміністратору)
class UsersPage : public QWidget
{
    Q_OBJECT
public:
    explicit UsersPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void addUser();
    void editSelected();
    void toggleActive();
    void resetPassword();

private:
    int selectedId() const;

    QTableView *m_view;
    QStandardItemModel *m_model;
};

#endif // USERSPAGE_H
