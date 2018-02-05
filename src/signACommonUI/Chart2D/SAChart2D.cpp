﻿#include "SAChart2D.h"
#include "SAChart.h"
#include "SAAbstractDatas.h"
#include "SARandColorMaker.h"
#include "SAFigureGlobalConfig.h"
#include "SAXYDataTracker.h"
#include "SAYDataTracker.h"
#include <memory>
#include "SAXYSeries.h"
#include "SABarSeries.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDebug>
#include "SAValueManagerMimeData.h"
#include "SAValueManager.h"
#include "SATendencyChartDataSelectDialog.h"
#include "SALog.h"
#include "SARectRegionSelectEditor.h"
#include "SAEllipseRegionSelectEditor.h"
#include "SAPolygonRegionSelectEditor.h"
#include <QUndoStack>
#include "SAFigureOptCommands.h"
#include "qwt_text.h"
#include "SASelectRegionShapeItem.h"
#include <numeric>
#include "SAScatterSeries.h"
#include "SABoxSeries.h"
#include "SAHistogramSeries.h"
#include "qwt_plot_intervalcurve.h"
#include "qwt_plot_multi_barchart.h"

int SAChart2D::s_size_pen_width2 = 1000;

class SAChart2DPrivate
{
    SA_IMPL_PUBLIC(SAChart2D)
public:
    QUndoStack m_undoStack;
    SAChart2D::SelectionMode m_selectMode;///< 选择模式
    SAAbstractRegionSelectEditor* m_chartSelectRigionEditor;///< 矩形选择编辑器
    QMap<QString,QPainterPath> m_selectionMap;///< 此dict保存的选区，选区可以保存，并加载
    SASelectRegionShapeItem* m_seclectRegionItem;///< 用于显示选区的item
    SAAbstractPlotEditor* m_editor;///< 额外的编辑器
    QList<QwtPlotItem*> m_currentSelectItem;///<当前选择的条目
    bool m_isStartPlotDrag;///<记录空格长按
    bool m_isEnablePannerBeforePressedSpace;///< 记录按下空格前是否处于panner状态
    bool m_isEnableZoomBeforePressedSpace;///< 记录按下空格前是否处于zoom状态
    QList<SAAbstractPlotMarker *> m_plotMarkers;
public:
    SAChart2DPrivate(SAChart2D* p):q_ptr(p)
      ,m_selectMode(SAChart2D::NoneSelection)
      ,m_chartSelectRigionEditor(nullptr)
      ,m_seclectRegionItem(nullptr)
      ,m_editor(nullptr)
      ,m_isStartPlotDrag(false)
      ,m_isEnablePannerBeforePressedSpace(false)
      ,m_isEnableZoomBeforePressedSpace(false)
    {

    }
    ~SAChart2DPrivate(){
        if(m_editor)
        {//m_editor有可能调用chart2D，而此时chart2D已经析构了SAChart2DPrivate，出现异常
          //因此，先析构m_editor
            delete m_editor;
            m_editor = nullptr;
        }
    }

    void startPlotDrag()
    {
        if(!m_isStartPlotDrag)
        {
            //说明第一次进入
            q_ptr->setCursor(Qt::OpenHandCursor);
            m_isEnablePannerBeforePressedSpace = q_ptr->isEnablePanner();
            m_isEnableZoomBeforePressedSpace = q_ptr->isEnableZoomer();
            q_ptr->enableZoomer(false);
            q_ptr->enablePanner(true);
        }
        m_isStartPlotDrag = true;
    }
    void endPlotDrag()
    {
        if(m_isStartPlotDrag)
        {
            //说明之前处于长按状态
            qDebug()<<"m_isEnableZoomBeforePressedSpace"<<m_isEnableZoomBeforePressedSpace;
            q_ptr->unsetCursor();
            if(!m_isEnablePannerBeforePressedSpace)
                q_ptr->enablePanner(false);
            if(m_isEnableZoomBeforePressedSpace)
                q_ptr->enableZoomer(true);
        }
        m_isStartPlotDrag = false;
    }

    void appendItemAddCommand(QwtPlotItem* item,const QString& des)
    {
        m_undoStack.push(new SAFigureChartItemAddCommand(q_ptr,item,des));
    }
    void appendItemDeleteCommand(QwtPlotItem* item,const QString& des)
    {
        m_undoStack.push(new SAFigureChartItemDeleteCommand(q_ptr,item,des));
    }
    void appendItemListAddCommand(QList<QwtPlotItem*> itemList,const QString& des)
    {
        bool isAutoReplot = q_ptr->autoReplot();
        q_ptr->setAutoReplot(false);
        m_undoStack.push(new SAFigureChartItemListAddCommand(q_ptr,itemList,des));
        q_ptr->setAutoReplot(isAutoReplot);
        q_ptr->replot();
    }
    void appendRemoveItemCommand(QwtPlotItem* item,const QString& des)
    {
        m_undoStack.push(new SAFigureChartItemDeleteCommand(q_ptr,item,des));
    }
    void appendAddSelectionRegionCommand(const QPainterPath& newRegion,const QString& des)
    {
        m_undoStack.push(new SAFigureChartSelectionRegionAddCommand(q_ptr,newRegion,des));
    }
    void appendRemoveChart2DItemDataInRangCommand(const QList<QwtPlotItem *>& items,const QString& des)
    {
        QUndoCommand* cmd = new QUndoCommand(des);
        cmd->setText(des);
        bool isAutoReplot = q_ptr->autoReplot();
        q_ptr->setAutoReplot(false);
        for(int i=0;i<items.size();++i)
        {
            QwtPlotItem *item = items[i];
            switch(items[i]->rtti())
            {
            case QwtPlotItem::Rtti_PlotCurve:
            case SA::RTTI_SAXYSeries:
            case SA::RTTI_SAScatterSeries:
            case QwtPlotItem::Rtti_PlotBarChart:
            case SA::RTTI_SABarSeries:
                if(QwtSeriesStore<QPointF>* cur = dynamic_cast<QwtSeriesStore<QPointF>*>(item))
                {
                    new SAFigureRemoveXYSeriesDataInRangCommand(q_ptr,cur,"",item->xAxis(),item->yAxis(),cmd);
                }
                break;
            case QwtPlotItem::Rtti_PlotSpectroCurve:
            case QwtPlotItem::Rtti_PlotIntervalCurve:
            case QwtPlotItem::Rtti_PlotHistogram:
            case QwtPlotItem::Rtti_PlotSpectrogram:
            case QwtPlotItem::Rtti_PlotTradingCurve:
            case QwtPlotItem::Rtti_PlotMultiBarChart:
            default:
                break;
            }
        }


        m_undoStack.push(cmd);
        q_ptr->setAutoReplot(isAutoReplot);
        q_ptr->replot();
    }


    void createRegionShapeItem()
    {
        if(nullptr == m_seclectRegionItem)
        {
            m_seclectRegionItem = new SASelectRegionShapeItem("region select item");
            m_seclectRegionItem->setZ(std::numeric_limits<double>::max());
            m_seclectRegionItem->attach(q_ptr);
        }
    }
    void releaseRegionShapeItem()
    {
        if(nullptr != m_seclectRegionItem)
        {
            m_seclectRegionItem->detach();
            delete m_seclectRegionItem;
            m_seclectRegionItem = nullptr;
        }
    }
};

SAChart2D::SAChart2D(QWidget *parent):SA2DGraph(parent)
  ,d_ptr(new SAChart2DPrivate(this))
{
    setAcceptDrops(true);
    canvas()->setFocusPolicy(Qt::ClickFocus);
}

SAChart2D::~SAChart2D()
{
    qDebug() <<"SAChart2D destroy";
}

QList<int> SAChart2D::getPlotItemsRTTI()
{
    QList<int> res;
    res<<QwtPlotItem::Rtti_PlotCurve
                <<QwtPlotItem::Rtti_PlotSpectroCurve
                <<QwtPlotItem::Rtti_PlotIntervalCurve
                <<QwtPlotItem::Rtti_PlotHistogram
                <<QwtPlotItem::Rtti_PlotBarChart
                <<QwtPlotItem::Rtti_PlotMultiBarChart
               << SA::RTTI_SAXYSeries
               << SA::RTTI_SABarSeries
               << SA::RTTI_SABoxSeries
               << SA::RTTI_SAHistogramSeries
               << SA::RTTI_SAScatterSeries
                  ;
    return res;
}

QList<int> SAChart2D::getXYSeriesItemsRTTI()
{
    QList<int> res;
    res<<QwtPlotItem::Rtti_PlotCurve
              <<QwtPlotItem::Rtti_PlotBarChart
               << SA::RTTI_SAXYSeries
               << SA::RTTI_SABarSeries
               << SA::RTTI_SAScatterSeries
                  ;
    return res;
}
#if 0
///
/// \brief item的类型判断，cureve bar 等绘图相关返回true
/// \param item
/// \return cureve bar 等绘图相关返回true
///
bool SAChart2D::isPlotChartItem(const QwtPlotItem *item)
{
    switch(item->rtti())
    {
    case QwtPlotItem::Rtti_PlotCurve:
    case QwtPlotItem::Rtti_PlotSpectroCurve:
    case QwtPlotItem::Rtti_PlotIntervalCurve:
    case QwtPlotItem::Rtti_PlotHistogram:
    case QwtPlotItem::Rtti_PlotSpectrogram:
    case QwtPlotItem::Rtti_PlotTradingCurve:
    case QwtPlotItem::Rtti_PlotBarChart:
    case QwtPlotItem::Rtti_PlotMultiBarChart:
    case SA::RTTI_SAXYSeries:
    case SA::RTTI_SABarSeries:
    case SA::RTTI_SABoxSeries:
    case SA::RTTI_SAHistogramSeries:
    case SA::RTTI_SAScatterSeries:
        return true;
    default:
        return false;
    }
    return false;
}
///
/// \brief 获取所有可支持的绘图条目
/// \param chart
/// \return
///
QwtPlotItemList SAChart2D::getPlotChartItemList(const QwtPlot *chart)
{
    const QwtPlotItemList& items = chart->itemList();
    QwtPlotItemList res;
    for(int i=0;i<items.size();++i)
    {
        if(SAChart::dynamicCheckIsPlotChartItem(items[i]))
        {
            res.append(items[i]);
        }
    }
    return res;
}
////
/// \brief 获取所有xy点的绘图条目包括QwtPlotCurve和SAXYSeries,SABarSeries,QwtBarChart
///
/// \param chart
/// \return
///
QwtPlotItemList SAChart2D::getPlotXYSeriesItemList(const QwtPlot *chart)
{
    return filterPlotItem(chart,{QwtPlotItem::Rtti_PlotCurve
                                ,QwtPlotItem::Rtti_PlotBarChart
                                ,SA::RTTI_SAXYSeries
                                ,SA::RTTI_SABarSeries
                                ,SA::RTTI_SAScatterSeries});
}
#endif
///
/// \brief 根据筛选set获取item list
/// \param chart
/// \param enableRtti
/// \return
///
QwtPlotItemList SAChart2D::filterPlotItem(const QwtPlot *chart, const QSet<int> &enableRtti)
{
    const QwtPlotItemList& items = chart->itemList();
    QwtPlotItemList res;
    for(int i=0;i<items.size();++i)
    {
        if(enableRtti.contains(items[i]->rtti()))
        {
            res.append(items[i]);
        }
    }
    return res;
}
///
/// \brief 获取item的颜色,无法获取单一颜色就返回QColor()
/// \param item
/// \return
///
QColor SAChart2D::getItemColor(const QwtPlotItem *item, const QColor &defaultClr)
{
    switch (item->rtti()) {
    case QwtPlotItem::Rtti_PlotCurve:
    case SA::RTTI_SAXYSeries:
    case SA::RTTI_SAScatterSeries:
    {
        const QwtPlotCurve* p = static_cast<const QwtPlotCurve*>(item);
        return p->pen().color();
    }
    case QwtPlotItem::Rtti_PlotIntervalCurve:
    {
        const QwtPlotIntervalCurve* p = static_cast<const QwtPlotIntervalCurve*>(item);
        return p->pen().color();
    }
    case QwtPlotItem::Rtti_PlotHistogram:
    case SA::RTTI_SAHistogramSeries:
    {
        const QwtPlotHistogram* p = static_cast<const QwtPlotHistogram*>(item);
        return p->brush().color();
    }
    case QwtPlotItem::Rtti_PlotBarChart:
    case SA::RTTI_SABarSeries:
    {
        const QwtPlotBarChart* bar = static_cast<const QwtPlotBarChart*>(item);
        const QwtColumnSymbol* symbol =  bar->symbol();
        if(symbol)
        {
            return symbol->palette().color(QPalette::Window);
        }
        break;
    }
    case QwtPlotItem::Rtti_PlotGrid:
    {
        const QwtPlotGrid* grid = static_cast<const QwtPlotGrid*>(item);
        return grid->majorPen().color();
    }
    case QwtPlotItem::Rtti_PlotMarker:
    {
        const QwtPlotMarker* marker = static_cast<const QwtPlotMarker*>(item);
        return marker->linePen ().color();
    }
    default:
        break;
    }
    return defaultClr;
}
///
/// \brief 获取item的数据个数
/// \param item
/// \return -1 is meaning nan
///
int SAChart2D::getItemDataSize(const QwtPlotItem *item)
{
    switch (item->rtti()) {
    case QwtPlotItem::Rtti_PlotCurve:
    case SA::RTTI_SAXYSeries:
    case SA::RTTI_SAScatterSeries:
    {
        const QwtPlotCurve* p = static_cast<const QwtPlotCurve*>(item);
        if(p)
        {
            return p->data()->size();
        }
        break;
    }
    case QwtPlotItem::Rtti_PlotIntervalCurve:
    {
        const QwtPlotIntervalCurve* p = static_cast<const QwtPlotIntervalCurve*>(item);
        if(p)
        {
            return p->data()->size();
        }
        break;
    }
    case QwtPlotItem::Rtti_PlotHistogram:
    case SA::RTTI_SAHistogramSeries:
    {
        const QwtPlotHistogram* p = static_cast<const QwtPlotHistogram*>(item);
        if(p)
        {
            return p->data()->size();
        }
    }
    case QwtPlotItem::Rtti_PlotBarChart:
    case SA::RTTI_SABarSeries:
    {
        const QwtPlotBarChart* p = static_cast<const QwtPlotBarChart*>(item);
        if(p)
        {
            return p->data()->size();
        }
    }
    case QwtPlotItem::Rtti_PlotMultiBarChart:
    {
        const QwtPlotMultiBarChart* p = static_cast<const QwtPlotMultiBarChart*>(item);
        if(p)
        {
            return p->data()->size();
        }
    }
    default:
        break;
    }
    return -1;
}

int SAChart2D::getPlotCurWidth(int size)
{
    if(size > s_size_pen_width2)
    {
        return 1;
    }
    return 2;
}

void SAChart2D::setThinLineWidthPointSize(int size)
{
    s_size_pen_width2 = size;
}

void SAChart2D::addItem(QwtPlotItem *item, const QString &des)
{
    d_ptr->appendItemAddCommand(item,des);
}

QwtPlotCurve *SAChart2D::addCurve(const double *xData, const double *yData, int size)
{
    if(size<=0)
    {
        return nullptr;
    }
    QwtPlotCurve* pCurve = nullptr;
    pCurve = new QwtPlotCurve;
    pCurve->setYAxis(yLeft);
    pCurve->setXAxis(xBottom);
    pCurve->setStyle(QwtPlotCurve::Lines);
    pCurve->setSamples(xData,yData,size);
    pCurve->setPen(SARandColorMaker::getCurveColor()
                ,getPlotCurWidth(pCurve->dataSize()));
    addItem(pCurve,tr("add curve:%1").arg(pCurve->title().text()));
    return pCurve;
}

QwtPlotCurve *SAChart2D::addCurve(const QVector<QPointF> &xyDatas)
{
    QwtPlotCurve* pCurve = nullptr;
    pCurve = new QwtPlotCurve;
    pCurve->setYAxis(yLeft);
    pCurve->setXAxis(xBottom);
    pCurve->setStyle(QwtPlotCurve::Lines);
    pCurve->setSamples(xyDatas);
    pCurve->setPen(SARandColorMaker::getCurveColor()
                ,getPlotCurWidth(pCurve->dataSize()));
    addItem(pCurve,tr("add curve:%1").arg(pCurve->title().text()));
    return pCurve;
}

QwtPlotCurve *SAChart2D::addCurve(const QVector<double> &xData, const QVector<double> &yData)
{
    QwtPlotCurve* pCurve = nullptr;
    pCurve = new QwtPlotCurve;
    pCurve->setYAxis(yLeft);
    pCurve->setXAxis(xBottom);
    pCurve->setStyle(QwtPlotCurve::Lines);
    pCurve->setSamples(xData,yData);
    pCurve->setPen(SARandColorMaker::getCurveColor()
                ,getPlotCurWidth(pCurve->dataSize()));
    addItem(pCurve,tr("add curve:%1").arg(pCurve->title().text()));
    return pCurve;
}

///
/// \brief 添加曲线
/// \param datas
/// \return
///
SAXYSeries *SAChart2D::addCurve(SAAbstractDatas *datas)
{
    QScopedPointer<SAXYSeries> series(new SAXYSeries(datas->getName(),datas));
    if(series->dataSize() <= 0)
    {
        return nullptr;
    }

    series->setPen(SARandColorMaker::getCurveColor()
                ,getPlotCurWidth(series->dataSize()));
    addItem(series.data(),tr("add curve:%1").arg(series->title().text()));
    return series.take();
}
///
/// \brief SAChart2D::addCurve
/// \param datas
/// \param xStart
/// \param xDetal
/// \return
///
SAXYSeries *SAChart2D::addCurve(SAAbstractDatas *datas, double xStart, double xDetal)
{
    QScopedPointer<SAXYSeries> series(new SAXYSeries(datas->getName()));
    series->setSamples(datas,xStart,xDetal);
    if(series->dataSize() <= 0)
    {
        return nullptr;
    }
    series->setPen(SARandColorMaker::getCurveColor()
                ,getPlotCurWidth(series->dataSize()));
    addItem(series.data(),tr("add curve:%1").arg(series->title().text()));
    return series.take();
}
///
/// \brief SAChart2D::addCurve
/// \param x
/// \param y
/// \param name
/// \return
///
SAXYSeries *SAChart2D::addCurve(SAAbstractDatas *x, SAAbstractDatas *y, const QString &name)
{
    QScopedPointer<SAXYSeries> series(new SAXYSeries(name));
    series->setSamples(x,y);
    if(series->dataSize() <= 0)
    {
        return nullptr;
    }
    series->setPen(SARandColorMaker::getCurveColor()
                ,getPlotCurWidth(series->dataSize()));
    addItem(series.data(),tr("add curve:%1").arg(series->title().text()));
    return series.take();
}





///
/// \brief SAChart2D::addBar
/// \param datas
/// \return
///
SAHistogramSeries *SAChart2D::addHistogram(SAAbstractDatas *datas)
{
    QScopedPointer<SAHistogramSeries> series(new SAHistogramSeries(datas,datas->getName()));
    if(series->dataSize() <= 0)
    {
        return nullptr;
    }
    QColor clr = SARandColorMaker::getCurveColor();
    series->setBrush(QBrush(clr));
    series->setPen(clr,getPlotCurWidth(series->dataSize()));
    series->setStyle(QwtPlotHistogram::Columns);
    addItem(series.data(),tr("add Histogram:%1").arg(series->title().text()));
    return series.take();
}


SABarSeries *SAChart2D::addBar(SAAbstractDatas *datas)
{
    QScopedPointer<SABarSeries> series(new SABarSeries(datas,datas->getName()));
    if(series->dataSize() <= 0)
    {
        return nullptr;
    }
    QColor clr = SARandColorMaker::getCurveColor();
    QwtColumnSymbol *symbol = new QwtColumnSymbol( QwtColumnSymbol::Box );
    QPalette p = symbol->palette();
    p.setColor(QPalette::Window,clr);
    symbol->setPalette(p);
    series->setSymbol(symbol);
    addItem(series.data(),tr("add bar:%1").arg(series->title().text()));
    return series.take();
}


///
/// \brief 绘制散点图-支持redo/undo
/// \param datas
/// \return
///
SAScatterSeries *SAChart2D::addScatter(SAAbstractDatas *datas)
{
    QScopedPointer<SAScatterSeries> series(new SAScatterSeries(datas->getName(),datas));
    if(series->dataSize() <= 0)
    {
        return nullptr;
    }
    QColor clr = SARandColorMaker::getCurveColor();
    QPen pen = series->pen();
    pen.setColor(clr);
    series->setPen(pen);
    addItem(series.data(),tr("add scatter:%1").arg(series->title().text()));
    return series.take();
}


///
/// \brief 绘制箱盒图
/// \param datas
/// \return
///
SABoxSeries *SAChart2D::addBox(SAAbstractDatas *datas)
{
    QScopedPointer<SABoxSeries> series(new SABoxSeries(datas,datas->getName()));
//    if(series->dataSize() <= 0)
//    {
//        return nullptr;
//    }
    addItem(series.data(),tr("add box:%1").arg(series->title().text()));
    return series.take();
}


///
/// \brief 添加一条竖直线
/// \return
///
QwtPlotMarker *SAChart2D::addVLine(double val)
{
    QwtPlotMarker *marker = new QwtPlotMarker();
    marker->setXValue(val);
    marker->setLineStyle( QwtPlotMarker::VLine );
    marker->setItemAttribute( QwtPlotItem::Legend, true );
    addItem(marker,tr("add VLine %1").arg(marker->title().text()));
    return marker;
}
///
/// \brief 添加一条水平线
/// \param val
/// \return
///
QwtPlotMarker *SAChart2D::addHLine(double val)
{
    QwtPlotMarker *marker = new QwtPlotMarker();
    marker->setYValue(val);
    marker->setLineStyle( QwtPlotMarker::HLine );
    marker->setItemAttribute( QwtPlotItem::Legend, true );
    addItem(marker,tr("add HLine %1").arg(marker->title().text()));
    return marker;
}

///
/// \brief 移除一个对象
/// \param item
///
void SAChart2D::removeItem(QwtPlotItem *item)
{
    d_ptr->appendRemoveItemCommand(item,tr("delete [%1]").arg(item->title().text()));
}

///
/// \brief 移除范围内数据
/// \param curves 需要移除的曲线列表
///
void SAChart2D::removeDataInRang(const QList<QwtPlotItem *>& chartItems)
{
    QPainterPath region = getSelectionRange();
    if(region.isEmpty())
    {
        return;
    }
    SAAbstractRegionSelectEditor* editor = getRegionSelectEditor();
    if(nullptr == editor)
    {
        return;
    }
    d_ptr->appendRemoveChart2DItemDataInRangCommand(chartItems,tr("remove rang data"));
#if 0
    QHash<QPair<int,int>,QPainterPath> otherScaleMap;
    for(int i=0;i<curves.size();++i)
    {
        int xa = curves[i]->xAxis();
        int ya = curves[i]->yAxis();
        if(xa == editor->getXAxis() && ya == editor->getYAxis())
        {
            SAChart::removeDataInRang(region,curves[i]);
        }
        else
        {
            QPair<int,int> axiss=qMakePair(xa,ya);
            if(!otherScaleMap.contains(axiss))
            {
                otherScaleMap[axiss] = editor->transformToOtherAxis(xa,ya);

            }
            SAChart::removeDataInRang(otherScaleMap.value(axiss)
                                      ,curves[i]);
        }
    }
    setAutoReplot(true);
    replot();
#endif
}

void SAChart2D::removeDataInRang()
{
    removeDataInRang(getCurrentSelectItems());
}
///
/// \brief 获取选择范围内的数据,如果当前没有选区，返回false
/// \param xy
/// \param index 如果非空，会把对应的索引填入
/// \param cur
/// \return 如果不是xy series会返回false，如果是，会返回true，无论选区里是否有数据都返回true，可以通过xy的size来判断获取了多少数据
///
bool SAChart2D::getXYDataInRange(QVector<QPointF> &xy, QVector<int>* index, const QwtPlotItem *cur)
{
    QPainterPath region = getSelectionRange();
    if(!isRegionVisible() || region.isEmpty())
    {
        return false;
    }
    SAAbstractRegionSelectEditor* editor = getRegionSelectEditor();
    if(nullptr == editor)
    {
        return false;
    }
    const QwtSeriesStore<QPointF>* series = dynamic_cast<const QwtSeriesStore<QPointF>*>(cur);
    if(nullptr == series)
    {
        return false;
    }
    int xa = cur->xAxis();
    int ya = cur->yAxis();
    if(xa != editor->getXAxis() && ya != editor->getYAxis())
    {
        region = editor->transformToOtherAxis(xa,ya);
    }
    SAChart::getXYDatas(xy,index,series,region);
    return true;
}
///
/// \brief 获取选择范围内的数据,如果当前没有选区，返回false
/// \param xs
/// \param ys
/// \param index
/// \param cur
/// \param index 如果非空，会把对应的索引填入
/// \return 如果不是xy series会返回false，如果是，会返回true，无论选区里是否有数据都返回true，可以通过x或y的size来判断获取了多少数据
///
bool SAChart2D::getXYDataInRange(QVector<double> *xs, QVector<double> *ys, QVector<int> *index, const QwtPlotItem *cur)
{
    QPainterPath region = getSelectionRange();
    if(!isRegionVisible() || region.isEmpty())
    {
        return false;
    }
    SAAbstractRegionSelectEditor* editor = getRegionSelectEditor();
    if(nullptr == editor)
    {
        return false;
    }
    const QwtSeriesStore<QPointF>* series = dynamic_cast<const QwtSeriesStore<QPointF>*>(cur);
    if(nullptr == series)
    {
        return false;
    }
    int xa = cur->xAxis();
    int ya = cur->yAxis();
    if(xa != editor->getXAxis() || ya != editor->getYAxis())
    {
        region = editor->transformToOtherAxis(xa,ya);
    }
    SAChart::getXYDatas(xs,ys,index,series,region);
    return true;
}
///
/// \brief 获取item对应的xy数据，如果可以转换的话
/// \param xy
/// \param cur
/// \return 如果不是xy series会返回false，如果是，会返回true
///
bool SAChart2D::getXYData(QVector<QPointF> &xy, const QwtPlotItem *cur)
{
    const QwtSeriesStore<QPointF>* series = dynamic_cast<const QwtSeriesStore<QPointF>*>(cur);
    if(nullptr == series)
    {
        return false;
    }
    SAChart::getXYDatas(xy,series);
    return true;
}
///
/// \brief 获取item对应的xy数据，如果可以转换的话
/// \param xs x值，可为空
/// \param ys y值，可为空
/// \param cur item
/// \return 如果不是xy series会返回false，如果是，会返回true
///
bool SAChart2D::getXYData(QVector<double> *xs, QVector<double> *ys, const QwtPlotItem *cur)
{
    const QwtSeriesStore<QPointF>* series = dynamic_cast<const QwtSeriesStore<QPointF>*>(cur);
    if(nullptr == series)
    {
        return false;
    }
    SAChart::getXYDatas(xs,ys,series);
    return true;
}


///
/// \brief 开始选择模式
/// 选择模式可分为矩形，圆形等，具体见\sa SelectionMode
/// \param mode 选择模式
/// \see SelectionMode
///
void SAChart2D::enableSelection(SelectionMode mode,bool on)
{
    SA_D(SAChart2D);
    if(on)
    {
        if(NoneSelection == mode)
        {
            return;
        }
        stopSelectMode();
        d->m_selectMode = mode;
        switch(mode)
        {
        case RectSelection:startRectSelectMode();break;
        case EllipseSelection:startEllipseSelectMode();break;
        case PolygonSelection:startPolygonSelectMode();break;
        default:return;
        }
    }
    else
    {
        stopSelectMode();
    }
}
///
/// \brief 判断当前的选择模式
/// \param mode
/// \return
///
bool SAChart2D::isEnableSelection(SAChart2D::SelectionMode mode) const
{
    SA_DC(SAChart2D);
    if(nullptr == d->m_chartSelectRigionEditor)
    {
        return false;
    }
    if(!d->m_chartSelectRigionEditor->isEnabled())
    {
        return false;
    }
    switch(mode)
    {
    case RectSelection:
        return d->m_chartSelectRigionEditor->rtti() == SAAbstractPlotEditor::RTTIRectRegionSelectEditor;
    case EllipseSelection:
        return d->m_chartSelectRigionEditor->rtti() == SAAbstractPlotEditor::RTTIEllipseRegionSelectEditor;
    case PolygonSelection:
        return d->m_chartSelectRigionEditor->rtti() == SAAbstractPlotEditor::RTTIPolygonRegionSelectEditor;
    default:
        break;
    }
    return false;
}
///
/// \brief 结束当前的选择模式
///
void SAChart2D::stopSelectMode()
{
    SA_D(SAChart2D);
    if(nullptr == d->m_chartSelectRigionEditor)
    {
        return;
    }
    d->m_chartSelectRigionEditor->setEnabled(false);
    d->m_selectMode = NoneSelection;
}
///
/// \brief 选区选择完成
/// \param shape
///
void SAChart2D::onSelectionFinished(const QPainterPath &shape)
{
    d_ptr->appendAddSelectionRegionCommand(shape,tr("add selection region"));
}
///
/// \brief 清除所有选区
///
void SAChart2D::clearAllSelectedRegion()
{
    SA_D(SAChart2D);
    if(nullptr == d->m_chartSelectRigionEditor)
    {
        return;
    }
    delete d->m_chartSelectRigionEditor;
    d->m_chartSelectRigionEditor = nullptr;
    d->releaseRegionShapeItem();
}
///
/// \brief 判断是否有选区
/// \return
///
bool SAChart2D::isRegionVisible() const
{
    SA_DC(SAChart2D);
    if(nullptr == d->m_seclectRegionItem)
    {
        return false;
    }
    return d->m_seclectRegionItem->isVisible();
}
///
/// \brief 获取当前正在显示的选择区域
/// \return
///
SAChart2D::SelectionMode SAChart2D::currentSelectRegionMode() const
{
    SA_DC(SAChart2D);
    return d->m_selectMode;
}
///
/// \brief 获取矩形选择编辑器
/// \return 如果没有设置编辑器，返回nullptr
///
SAAbstractRegionSelectEditor *SAChart2D::getRegionSelectEditor()
{
    SA_D(SAChart2D);
    return d->m_chartSelectRigionEditor;
}

const SAAbstractRegionSelectEditor *SAChart2D::getRegionSelectEditor() const
{
    SA_DC(SAChart2D);
    return d->m_chartSelectRigionEditor;
}
///
/// \brief 获取当前可见的选区的范围
/// \return
///
QPainterPath SAChart2D::getSelectionRange() const
{
    SA_DC(SAChart2D);
    if(!d->m_seclectRegionItem)
    {
        return QPainterPath();
    }
    return d->m_seclectRegionItem->shape();
}
///
/// \brief 获取当前可见的选区的范围,选区范围更加坐标轴进行映射
/// \param xaxis
/// \param yaxis
/// \return
///
QPainterPath SAChart2D::getSelectionRange(int xaxis, int yaxis) const
{
    QPainterPath shape = getSelectionRange();

    QwtScaleMap xRawMap = canvasMap(axisEnabled(QwtPlot::yLeft) ? QwtPlot::yLeft : QwtPlot::yRight);
    QwtScaleMap yRawMap = canvasMap(axisEnabled(QwtPlot::xBottom) ? QwtPlot::xBottom : QwtPlot::xTop);
    const SAAbstractRegionSelectEditor* editor = getRegionSelectEditor();
    if(editor)
    {
        if(xaxis == editor->getXAxis() && yaxis == editor->getYAxis())
        {
            return shape;
        }
        return editor->transformToOtherAxis(xaxis,yaxis);
    }
    QwtScaleMap xMap = canvasMap( xaxis );
    QwtScaleMap yMap = canvasMap( yaxis );

    const int eleCount = shape.elementCount();
    for(int i=0;i<eleCount;++i)
    {
        const QPainterPath::Element &el = shape.elementAt( i );
        QPointF tmp = QwtScaleMap::transform(xRawMap,yRawMap,QPointF(el.x,el.y));
        tmp = QwtScaleMap::invTransform(xMap,yMap,tmp);
        shape.setElementPositionAt( i, tmp.x(), tmp.y() );
    }
    return shape;
}

void SAChart2D::setSelectionRange(const QPainterPath &painterPath)
{
    SA_D(SAChart2D);
    if(!d->m_seclectRegionItem)
    {
        d->createRegionShapeItem();
    }
    d->m_seclectRegionItem->setShape(painterPath);
    if(!d->m_seclectRegionItem->isVisible())
    {
        d->m_seclectRegionItem->setVisible(true);
    }
    emit selectionRegionChanged(painterPath);
}
///
/// \brief 判断当前的条目的x，y轴关联是否和选区的一致
/// \param item
/// \return 如果没有选区，或者不一致，返回false
///
bool SAChart2D::isSelectionRangeAxisMatch(const QwtPlotItem *item)
{
    SAAbstractRegionSelectEditor* editor = getRegionSelectEditor();
    if(!editor)
    {
        return false;
    }
    return (editor->getXAxis() == item->xAxis()) && (editor->getYAxis() == item->yAxis());
}
///
/// \brief redo
///
void SAChart2D::redo()
{
    d_ptr->m_undoStack.redo();
}
///
/// \brief undo
///
void SAChart2D::undo()
{
    d_ptr->m_undoStack.undo();
}

void SAChart2D::appendCommand(SAFigureOptCommand *cmd)
{
    d_ptr->m_undoStack.push(cmd);
}
///
/// \brief 设置一个编辑器，编辑器的内存交由SAChart2D管理，SAChart2D只能存在一个额外的编辑器
///
/// \param editor 传入null将解除之前的editor
///
void SAChart2D::setEditor(SAAbstractPlotEditor *editor)
{
    if(d_ptr->m_editor)
    {
        delete (d_ptr->m_editor);
    }
    if(editor)
    {
        if(this != editor->parent())
        {
            editor->setParent(this);
        }
    }
    d_ptr->m_editor = editor;
}
///
/// \brief 获取当前的编辑器
/// \return
///
SAAbstractPlotEditor *SAChart2D::getEditor() const
{
    return d_ptr->m_editor;
}
///
/// \brief 当前选择的条目
/// \return
///
const QList<QwtPlotItem *> &SAChart2D::getCurrentSelectItems() const
{
    SA_DC(SAChart2D);
    return d->m_currentSelectItem;
}

///
/// \brief 判断当前选中的条目是否有曲线
/// \return
///
bool SAChart2D::isCurrentSelectItemsHaveChartItem() const
{
    const QList<QwtPlotItem *> &items = getCurrentSelectItems();
    for(int i=0;i<items.size();++i)
    {
        if(SAChart::dynamicCheckIsPlotChartItem(items[i]))
        {
            return true;
        }
    }
    return false;
}

void SAChart2D::setCurrentSelectItems(const QList<QwtPlotItem *> &items)
{
    SA_D(SAChart2D);
    d->m_currentSelectItem = items;
    emit currentSelectItemsChanged(items);
}

void SAChart2D::setCurrentSelectPlotCurveItems(const QList<QwtPlotCurve *> &items)
{
    SA_D(SAChart2D);
    d->m_currentSelectItem.clear();
    for(int i=0;i<items.size();++i)
    {
        d->m_currentSelectItem.append(items[i]);
    }
    emit currentSelectItemsChanged(d->m_currentSelectItem);
}
///
/// \brief 把当前存在的编辑器禁止
///
void SAChart2D::unenableEditor()
{
    if(d_ptr->m_chartSelectRigionEditor)
    {
        d_ptr->m_chartSelectRigionEditor->setEnabled(false);
    }
    if(SAAbstractPlotEditor* editor = getEditor())
    {
        editor->setEnabled(false);
    }
    SA2DGraph::unenableEditor();
}
///
/// \brief 添加标记 通过此函数添加的标记将会记录到一个列表中
/// \param marker
///
void SAChart2D::addPlotMarker(SAAbstractPlotMarker *marker)
{
    d_ptr->m_plotMarkers.append(marker);
    marker->attach(this);
}
///
/// \brief 返回此图记录的标记
/// \return
///
const QList<SAAbstractPlotMarker *> &SAChart2D::getPlotMarkers() const
{
    return d_ptr->m_plotMarkers;
}
///
/// \brief 移除此图记录的一个标记
/// \param marker
///
void SAChart2D::removePlotMarker(SAAbstractPlotMarker *marker)
{
    int index = d_ptr->m_plotMarkers.indexOf(marker);
    if(index >= 0)
    {
        SAAbstractPlotMarker *m = d_ptr->m_plotMarkers.takeAt(index);
        m->detach();
        delete m;
    }
}
///
/// \brief 移除所有标记
///
void SAChart2D::removeAllPlotMarker()
{
    std::for_each(d_ptr->m_plotMarkers.begin(),d_ptr->m_plotMarkers.end()
                  ,[](SAAbstractPlotMarker *marker){
        marker->detach();
        delete marker;
    });
    d_ptr->m_plotMarkers.clear();
}

///
/// \brief 开始矩形选框模式
///
void SAChart2D::startRectSelectMode()
{
    SA_D(SAChart2D);
    if(d->m_chartSelectRigionEditor)
    {
        delete d->m_chartSelectRigionEditor;
        d->m_chartSelectRigionEditor = nullptr;
    }
    if(nullptr == d->m_chartSelectRigionEditor)
    {
        d->m_chartSelectRigionEditor = new SARectRegionSelectEditor(this);
        connect(d->m_chartSelectRigionEditor,&SAAbstractRegionSelectEditor::finishSelection
                ,this,&SAChart2D::onSelectionFinished);
        d->m_chartSelectRigionEditor->setSelectRegion(getSelectionRange());
    }
    d->m_chartSelectRigionEditor->setEnabled(true);
}
///
/// \brief 开始椭圆选框模式
///
void SAChart2D::startEllipseSelectMode()
{
    SA_D(SAChart2D);
    if(d->m_chartSelectRigionEditor)
    {
        delete d->m_chartSelectRigionEditor;
        d->m_chartSelectRigionEditor = nullptr;
    }
    if(nullptr == d->m_chartSelectRigionEditor)
    {
        d->m_chartSelectRigionEditor = new SAEllipseRegionSelectEditor(this);
        connect(d->m_chartSelectRigionEditor,&SAAbstractRegionSelectEditor::finishSelection
                ,this,&SAChart2D::onSelectionFinished);
        d->m_chartSelectRigionEditor->setSelectRegion(getSelectionRange());
    }
    d->m_chartSelectRigionEditor->setEnabled(true);
}
///
/// \brief 开始多边形选框模式
///
void SAChart2D::startPolygonSelectMode()
{
    SA_D(SAChart2D);
    if(d->m_chartSelectRigionEditor)
    {
        delete d->m_chartSelectRigionEditor;
        d->m_chartSelectRigionEditor = nullptr;
    }
    if(nullptr == d->m_chartSelectRigionEditor)
    {
        d->m_chartSelectRigionEditor = new SAPolygonRegionSelectEditor(this);
        connect(d->m_chartSelectRigionEditor,&SAAbstractRegionSelectEditor::finishSelection
                ,this,&SAChart2D::onSelectionFinished);
        d->m_chartSelectRigionEditor->setSelectRegion(getSelectionRange());
    }
    d->m_chartSelectRigionEditor->setEnabled(true);
}

///
/// \brief 向chart添加一组数据
/// \param datas
///
QList<SAXYSeries *> SAChart2D::addDatas(const QList<SAAbstractDatas *> &datas)
{
    saPrint();
    QList<SAXYSeries *> res;
    const int dataCount = datas.size();
    if(1 == dataCount)
    {
        if(SA::Dim1 == datas[0]->getDim())
        {
            //1维向量
            SATendencyChartDataSelectDialog dlg(this);
            if(QDialog::Accepted != dlg.exec())
            {
                return QList<SAXYSeries *>();
            }

            QList<QwtPlotItem*> itemList;
            if(dlg.isFollow())
            {
                QwtPlotCurve* cur = dlg.getSelFollowCurs();
                std::unique_ptr<SAXYSeries> series(new SAXYSeries(datas[0]->getName()));
                QVector<double> x,y;
                if(!SAAbstractDatas::converToDoubleVector(datas[0],y))
                {
                    return QList<SAXYSeries *>();
                }
                SAChart::getXYDatas(&x,nullptr,cur);
                if(x.size() < y.size())
                {
                    y.resize(x.size());
                }
                else
                {
                    x.resize(y.size());
                }
                series->setSamples(x,y);
                series->setPen(SARandColorMaker::getCurveColor()
                            ,getPlotCurWidth(series->dataSize()));
                itemList.append(series.release());
            }
            else if(dlg.isSelDef())
            {
                double startData,addedData;
                dlg.getSelDefData(startData,addedData);
                SAXYSeries* seris = addCurve(datas[0],startData,addedData);
                res.append(seris);
            }

            if(itemList.size() > 0)
            {
                d_ptr->appendItemListAddCommand(itemList,tr("add datas"));
                for(int i=0;i<itemList.size();++i)
                {
                    res.append(static_cast<SAXYSeries*>(itemList[i]));
                }
            }
            return res;
        }
        else
        {
           SAXYSeries* seris = addCurve(datas[0]);
           if(seris)
           {
                res.append(seris);
           }
        }
    }
    else if (2 == dataCount)
    {
        if(SA::Dim1 == datas[0]->getDim()
                &&
                SA::Dim1 == datas[1]->getDim())
        {
            SAXYSeries* seris = addCurve(datas[0],datas[1]
                    ,QString("%1-%2")
                    .arg(datas[0]->getName())
                    .arg(datas[1]->getName()));
            res.append(seris);
        }
    }
    else
    {
        setAutoReplot(false);
        std::for_each(datas.cbegin(),datas.cend(),[this,&res](SAAbstractDatas* data){
            SAXYSeries* seris = addCurve(data);
            if(seris)
            {
                res.append(seris);
            }
        });
        setAutoReplot(true);
        replot();
    }
    return res;
}

void SAChart2D::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void SAChart2D::dragMoveEvent(QDragMoveEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void SAChart2D::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        QList<int> ids;
        if(SAValueManagerMimeData::getValueIDsFromMimeData(event->mimeData(),ids))
        {
            QList<SAAbstractDatas*> datas = saValueManager->findDatas(ids);
            addDatas(datas);
        }
    }
}




void SAChart2D::keyPressEvent(QKeyEvent *e)
{
    if(e)
    {
        if(Qt::Key_Space == e->key() && !d_ptr->m_isStartPlotDrag)
        {
            //记录空格长按
            d_ptr->startPlotDrag();
        }
    }
    return SA2DGraph::keyPressEvent(e);
}

void SAChart2D::keyReleaseEvent(QKeyEvent *e)
{
    if(e)
    {
        //qDebug() << "keyReleaseEvent isAutoRepeat"<<e->isAutoRepeat()<<" key:"<<e->key();
        if(!e->isAutoRepeat() && d_ptr->m_isStartPlotDrag)
        {
            if(Qt::Key_Space == e->key())
            {
                d_ptr->endPlotDrag();
            }
        }
    }
    return SA2DGraph::keyReleaseEvent(e);
}


