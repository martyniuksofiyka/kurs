#ifndef CATALOGPAGE_H
#define CATALOGPAGE_H

#include <QWidget>

class QStandardItemModel;
class QTabWidget;
class QTableView;
class TableProxy;

// Довідники: зали студії та реквізит
class CatalogPage : public QWidget
{
    Q_OBJECT
public:
    explicit CatalogPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void add();
    void editSelected();
    void toggleActive();

private:
    bool hallsTab() const;
    int selectedId() const;
    bool editHall(int id);
    bool editProp(int id);

    QTabWidget *m_tabs;
    QTableView *m_hallsView;
    QTableView *m_propsView;
    QStandardItemModel *m_halls;
    QStandardItemModel *m_props;
    TableProxy *m_hallsProxy;
    TableProxy *m_propsProxy;
};

#endif // CATALOGPAGE_H
