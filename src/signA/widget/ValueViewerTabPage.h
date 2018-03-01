﻿#ifndef VALUEVIEWERTABPAGE_H
#define VALUEVIEWERTABPAGE_H

#include <QWidget>
#include <SAData.h>
#include <QList>
#include <QHash>
#include <memory>
#include <QMenu>
#include <QItemSelectionModel>
namespace Ui {
class ValueViewerTabPage;
}

class SADataTableModel;
class QTableView;
class QWheelEvent;
///
/// \brief sa的表格窗体
///
class ValueViewerTabPage : public QWidget
{
    Q_OBJECT

public:
    explicit ValueViewerTabPage(QWidget *parent = 0);
    ~ValueViewerTabPage();
    QTableView* getTableView();
    SADataTableModel* getModel() const;
private slots:
    void on_tableView_customContextMenuRequested(const QPoint &pos);
    //选择的列转换为向量
    void on_actionToLinerData_triggered();
    //选择的列转换为点序列
    void on_actionToPointFVectorData_triggered();
    //
    void onTableViewDoubleClicked(const QModelIndex& index);
private:
    bool setData(int r,int c,const QVariant& v);
    void getSelectLinerData(QHash<int, QVector<double> >& rawData) const;
    void getSelectVectorPointData(QVector< std::shared_ptr<QVector<QPointF> > > &rawData,int dim = 0);
    bool getSelectVectorPointData(SAVectorPointF* data);
    void appendDataFromVariant(const QVariant& var,QVector<double>& data) const;
    //获取选择的列值
    static void getQItemSelectionColumns(QItemSelectionModel* selModel
                                         ,QMap<int,std::shared_ptr<QVector<QVariant> > >& res);
    void wheelEvent(QWheelEvent * event);
private:
    Ui::ValueViewerTabPage *ui;
    //OpenFileManager* m_values;
    uint m_countNewData;
    QMenu* m_menu;
};

#endif // VALUEVIEWERTABPAGE_H
