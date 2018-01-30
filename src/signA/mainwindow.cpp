﻿#include "mainwindow.h"

#ifdef SA_USE_RIBBON_UI
#include "MainWindowPrivate.h"
#else
#include "ui_mainwindow.h"
#endif

#include <functional>
#include <QElapsedTimer>
//#include "TxtQuickImportWizDlg.h"
//----------Qt---------------
#include <QMessageBox>
#include <QPluginLoader>
#include <QFileDialog>
#include <QColorDialog>
#include <QDate>
#include <QLocale>
#include <QTextCodec>
#include <QItemSelectionModel>
#include <QElapsedTimer>
#include <QSettings>
#include <QInputDialog>
#include <QInputDialog>
#include <QMdiArea>
#include <QProcess>
//----------STL-------------
#include <iostream>
#include <algorithm>
#include <limits>
#include <functional>
#include <memory>

//----------SA--------------

// |------Dialog------------
#include <Dialog_AddChart.h>
#include "CurveSelectDialog.h"
#include "SAProjectInfomationSetDialog.h"


#include <PickCurveDataModeSetDialog.h>
#include <SAPropertySetDialog.h>

// |------widget
#include <SATabValueViewerWidget.h>
#include <SAMessageWidget.h>
#include "SAFigureWindow.h"
// |------操作

//===signACommonUI
#include "SAUI.h"
#include "SAUIReflection.h"//ui管理器
#include "SAValueSelectDialog.h"
#include "SASelectRegionEditor.h"
#include "SASelectRegionDataEditor.h"
#include "SAMdiSubWindowSerializeHead.h"
//===signALib
#include "SALocalServerDefine.h"
#include "SAValueManager.h"//变量管理
#include "SAValueManagerModel.h"//变量管理的model
#include "SAPluginInterface.h"
#include "SALog.h"
#include "SAProjectManager.h"
#include "SAValueManagerMimeData.h"
//===SAChart
#include "SAChart.h"
#include "SAQwtSerialize.h"

#include <SAThemeManager.h>

// |------代理
//#include <SAFuctionDelegate.h>

// |-----chart
#include <SAPlotMarker.h>
#include "SAChart3D.h"
// |-----common ui
#include "SAChart2D.h"
// |-----宏-----------------
#include "DebugInfo.h"
// |-----czy
#include <czyQtUI.h>
#include <czyQtApp.h>
#include <czyMath_Interpolation.h>
// |-----model class --------------
#include <MdiWindowModel.h>
#include <QwtPlotItemDataModel.h>
#include <QwtPlotItemTreeModel.h>
#include "DataFeatureTreeModel.h"
#include <SAVariantHashTableModel.h>
#include <SACsvFileTableModel.h>
#include <SAPlotLayerModel.h>
//--------3thparty-----------
#define TR(str) \
    QApplication::translate("MainWindow", str, 0)

//
//--使用策略模式构建sa里的所有方法
//--改Global里，用SA的枚举替换所有的宏

//#include <SAFigure.h>




//#include <qwt3d_gridplot.h>
//#include <layerItemDelegate.h>


//--删除表格数据发生错误
static QColor static_line_color[9] = {
    Qt::black
    ,Qt::red
    ,Qt::green
    ,Qt::blue
    ,Qt::cyan
    ,Qt::magenta
    ,Qt::yellow
    ,Qt::gray
    ,Qt::lightGray
};

QColor MainWindow::getRandColor()
{
    return static_line_color[int (float(qrand())/float(RAND_MAX)*9.0)];
}

void debug();









MainWindow::MainWindow(QWidget *parent) :
#ifdef SA_USE_RIBBON_UI
    SARibbonMainWindow(parent),
    ui(new MainWindowPrivate(this))
#else
    QMainWindow(parent),
    ui(new Ui::MainWindow)
#endif
  ,m_uiInterface(new SAUI(this))
  ,ui_status_progress(nullptr)
  ,ui_status_info(nullptr)
  ,m_menuTreeProjectItem(nullptr)
  ,m_menuValueManagerView(nullptr)
 // ,m_lastActiveWnd(nullptr)
  ,m_nProjectCount(0)
  ,m_lastShowFigureWindow(nullptr)
{
    saAddLog("start app");
    saStartElapsed("start main app init");
#ifdef SA_USE_RIBBON_UI
    ui->init();
#else
    ui->setupUi(this);
#endif
    QWidget* p = takeCentralWidget();
    if(p)
        delete p;//移除中央窗口
    init();
    initUI();
    initMenu();
    initUIReflection();
    initProcess();
    saElapsed("init ui and menu");
    saStartElapsed("start load plugin and theme");
#ifndef SA_USE_RIBBON_UI
    ui->toolBar_chartSet->setEnabled(false);
#endif
    initPlugin();
    initTheme();
    saElapsed("loaded plugins and themes");
    showNormalMessageInfo(QStringLiteral("程序初始化完成"));
    loadSetting();
}


void debug(){

}

///
/// \brief 程序初始化
///
void MainWindow::init()
{
	//状态栏的进度条

    ui_status_info = new SAInformationStatusWidget(this);
    ui->statusBar->addWidget(ui_status_info);
    ui_status_info->setVisible (false);
    ui_status_info->setFadeIn (true,500,5);
    ui_status_info->setFadeOut (true,500,5);
    ui_status_progress = new progressStateWidget(this);
    ui->statusBar->addWidget(ui_status_progress);
    ui_status_progress->setVisible(false);
}

    ///
/// \brief 界面初始化
///
void MainWindow::initUI()
{
    connect(qApp,&QApplication::focusChanged,this,&MainWindow::onFocusChanged);

    setDockNestingEnabled(true);
    setDockOptions(QMainWindow::AnimatedDocks|QMainWindow::AllowTabbedDocks|QMainWindow::AllowNestedDocks);
    //mdi窗口管理器关联
    m_mdiManager.setMdi(ui->mdiArea);
    //项目结构树
    m_drawDelegate.reset (new SADrawDelegate(this));

	//////////////////////////////////////////////////////////////////////////
	//model
	//////////////////////////////////////////////////////////////////////////
    //子窗口列表的model
    MdiWindowModel* mdiListModel = new MdiWindowModel(ui->mdiArea,ui->listView_window);
    ui->listView_window->setModel(mdiListModel);
    connect(ui->listView_window,&QAbstractItemView::clicked,mdiListModel,&MdiWindowModel::onItemClick);
    connect(ui->listView_window,&QAbstractItemView::doubleClicked,mdiListModel,&MdiWindowModel::onItemDoubleClick);
    //-------------------------------------
    // - 图层的设置
	SAPlotLayerModel* layerModel = new SAPlotLayerModel(ui->tableView_layer);
    QHeaderView* plotLayerVerticalHeader = ui->tableView_layer->verticalHeader();
    if(plotLayerVerticalHeader)
    {
        plotLayerVerticalHeader->setDefaultSectionSize(19);
    }
	ui->tableView_layer->setModel(layerModel);
    ui->tableView_layer->setSelectionMode(QAbstractItemView::ExtendedSelection);
	auto hh = ui->tableView_layer->horizontalHeader();
	hh->setSectionResizeMode(0,QHeaderView::ResizeToContents);
	hh->setSectionResizeMode(1,QHeaderView::ResizeToContents);
	hh->setSectionResizeMode(2,QHeaderView::ResizeToContents);
	hh->setStretchLastSection(true);
    connect(ui->tableView_layer,&QTableView::pressed,this,&MainWindow::onTableViewLayerPressed);
    //------------------------------------------------------------
    //- dataviewer dock 数据观察dock的相关设置
    QwtPlotItemDataModel* qwtDataModel = new QwtPlotItemDataModel(this);
    ui->tableView_curSelItemDatas->setModel(qwtDataModel);
    QwtPlotItemTreeModel* qwtItemTreeModel = new QwtPlotItemTreeModel(this);
    ui->treeView_curPlotItem->setModel(qwtItemTreeModel);
    qwtItemTreeModel->setFilter (QList<QwtPlotItem::RttiValues>()
                          <<QwtPlotItem::Rtti_PlotCurve
                          <<QwtPlotItem::Rtti_PlotBarChart
                          <<QwtPlotItem::Rtti_PlotHistogram
                          <<QwtPlotItem::Rtti_PlotIntervalCurve
                          <<QwtPlotItem::Rtti_PlotTradingCurve
                          );
    ui->splitter_chartDataViewer->setStretchFactor(0,1);
    ui->splitter_chartDataViewer->setStretchFactor(1,3);
    //-------------------------------------
    // - start valueManager signal/slots connect
    connect(ui->treeView_valueManager,&QTreeView::clicked,this,&MainWindow::onTreeViewValueManagerClicked);
    connect(ui->treeView_valueManager,&QTreeView::doubleClicked,this,&MainWindow::onTreeViewValueManagerDoubleClicked);
    connect(ui->treeView_valueManager,&QTreeView::customContextMenuRequested,this,&MainWindow::onTreeViewValueManagerCustomContextMenuRequested);
    connect(ui->actionRenameValue,&QAction::triggered,this,&MainWindow::onActionRenameValueTriggered);
    //-------------------------------------
    // - start subwindow signal/slots connect
    connect(ui->mdiArea,&QMdiArea::subWindowActivated,this,&MainWindow::onMdiAreaSubWindowActivated);
    //-------------------------------------
    // - start file menu signal/slots connect
    connect(ui->actionOpen,&QAction::triggered,this,&MainWindow::onActionOpenTriggered);
    connect(ui->actionOpenProject,&QAction::triggered,this,&MainWindow::onActionOpenProjectTriggered);
    connect(ui->actionSave,&QAction::triggered,this,&MainWindow::onActionSaveTriggered);
    connect(ui->actionSaveAs,&QAction::triggered,this,&MainWindow::onActionSaveAsTriggered);
    connect(ui->actionClearProject,&QAction::triggered,this,&MainWindow::onActionClearProjectTriggered);
    //-------------------------------------
    // - start chart set menu signal/slots connect
    connect(ui->actionNewChart,&QAction::triggered,this,&MainWindow::onActionNewChartTriggered);
    connect(ui->actionNewTrend,&QAction::triggered,this,&MainWindow::onActionAddLineChartTriggered);
    connect(ui->actionDrawBarChart,&QAction::triggered,this,&MainWindow::onActionAddBarChartTriggered);
    connect(ui->actionDrawHistogramChart,&QAction::triggered,this,&MainWindow::onActionAddHistogramChartTriggered);
    connect(ui->actionDrawScatterChart,&QAction::triggered,this,&MainWindow::onActionAddScatterChartTriggered);
    connect(ui->actionDrawBoxChart,&QAction::triggered,this,&MainWindow::onActionAddBoxChartTriggered);

    //-------------------------------------
    // - menu_chartDataManager menu signal/slots connect
    connect(ui->actionInRangDataRemove,&QAction::triggered,this,&MainWindow::onActionInRangDataRemoveTriggered);
    connect(ui->actionPickCurveToData,&QAction::triggered,this,&MainWindow::onActionPickCurveToDataTriggered);
    //-------------------------------------
    // - menu_dataManager menu signal/slots connect
    connect(ui->actionViewValueInCurrentTab,&QAction::triggered,this,&MainWindow::onActionViewValueInCurrentTabTriggered);
    connect(ui->actionViewValueInNewTab,&QAction::triggered,this,&MainWindow::onActionViewValueInNewTabTriggered);
    connect(ui->actionDeleteValue,&QAction::triggered,this,&MainWindow::onActionDeleteValueTriggered);
    //-------------------------------------
    // - about menu signal/slots connect
    connect(ui->actionAbout,&QAction::triggered,this,&MainWindow::onActionAboutTriggered);
    //-------------------------------------
    // - TreeView CurPlotItem slots(曲线条目树形窗口)
    connect(ui->treeView_curPlotItem,&QTreeView::clicked,this,&MainWindow::onTreeViewCurPlotItemClicked);
    //-------------------------------------
    // - TreeView CurPlotItem slots(曲线条目树形窗口)
    connect(ui->actionUndo,&QAction::triggered,this,&MainWindow::onActionUndoTriggered);
    connect(ui->actionRedo,&QAction::triggered,this,&MainWindow::onActionRedoTriggered);
    //-------------------------------------
    // - tool menu signal/slots connect
    connect(ui->actionProjectSetting,&QAction::triggered,this,&MainWindow::onActionProjectSettingTriggered);
    //------------------------------------------------------------
    //- window menu 窗口 菜单
    connect(ui->actionSetDefalutDockPos,&QAction::triggered,this,&MainWindow::onActionSetDefalutDockPosTriggered);
    //窗口模式
    connect(ui->actionWindowMode,&QAction::triggered,this,&MainWindow::onActionWindowModeTriggered);
    //标签模式
    connect(ui->actionTabMode,&QAction::triggered,this,&MainWindow::onActionTabModeTriggered);
    ui->actionTabMode->setChecked(true);//默认标签模式
    //层叠布置
    connect(ui->actionWindowCascade,&QAction::triggered,this,&MainWindow::onActionWindowCascadeTriggered);
    //均匀布置
    connect(ui->actionWindowTile,&QAction::triggered,this,&MainWindow::onActionWindowTileTriggered);
    //======================================================
    //显示隐藏dock窗口
    //显示隐藏DataFeatureDock窗口
    connect(ui->actionDataFeatureDock,&QAction::triggered,this,&MainWindow::onActionDataFeatureDockTriggered);
    //显示隐藏SubWindowListDock窗口
    connect(ui->actionSubWindowListDock,&QAction::triggered,this,&MainWindow::onActionSubWindowListDockTriggered);
    //显示隐藏ValueManagerDock窗口
    connect(ui->actionValueManagerDock,&QAction::triggered,this,&MainWindow::onActionValueManagerDockTriggered);
    //显示隐藏LayerOutDock窗口
    connect(ui->actionLayerOutDock,&QAction::triggered,this,&MainWindow::onActionLayerOutDockTriggered);
    //显示隐藏ValueViewerDock窗口
    connect(ui->actionValueViewerDock,&QAction::triggered,this,&MainWindow::onActionValueViewerDockTriggered);
    //显示隐藏FigureViewer窗口
    connect(ui->actionFigureViewer,&QAction::triggered,this,&MainWindow::onActionFigureViewerTriggered);
    //显示隐藏message窗口
    connect(ui->actionMessageInfoDock,&QAction::triggered,this,&MainWindow::onActionMessageInfoDockTriggered);

    //===========================================================
    //- 图表设置菜单及工具栏的关联
    //十字光标
    ui->actionEnableChartCrossCursor->setCheckable(true);
    connect(ui->actionEnableChartCrossCursor,&QAction::triggered
            ,this,&MainWindow::onActionEnableChartPickerTriggered);

    //拖动
    ui->actionEnableChartPanner->setCheckable(true);
    connect(ui->actionEnableChartPanner,&QAction::triggered
            ,this,&MainWindow::onActionEnableChartPannerTriggered);
    //区间缩放
    ui->actionEnableChartZoom->setCheckable(true);
    connect(ui->actionEnableChartZoom,&QAction::triggered
            ,this,&MainWindow::onActionEnableChartZoomTriggered);

#ifndef SA_USE_RIBBON_UI
    QToolButton* toolbtn = qobject_cast<QToolButton*>(ui->toolBar_chartSet->widgetForAction(ui->actionEnableChartZoom));
    if(toolbtn)
    {
        QMenu* m1 = new QMenu(toolbtn);
        m1->addAction(ui->actionZoomIn);
        m1->addAction(ui->actionZoomOut);
        m1->addAction(ui->actionZoomBase);
        m1->addAction(ui->actionChartZoomReset);
        toolbtn->setPopupMode(QToolButton::MenuButtonPopup);
        toolbtn->setMenu(m1);
    }
#endif
    connect(ui->actionSetZoomBase,&QAction::triggered
            ,this,&MainWindow::onActionSetChartZoomToBaseTriggered);
    connect(ui->actionChartZoomReset,&QAction::triggered
            ,this,&MainWindow::onActionChartZoomResetTriggered);
    connect(ui->actionZoomIn,&QAction::triggered
            ,this,&MainWindow::onActionChartZoomInTriggered);
    connect(ui->actionZoomOut,&QAction::triggered
            ,this,&MainWindow::onActionChartZoomOutTriggered);
    //选区菜单
    m_chartRegionSelectionShapeActionGroup = new QActionGroup(this);
    ui->actionStartRectSelect->setActionGroup(m_chartRegionSelectionShapeActionGroup);
    ui->actionStartEllipseSelect->setActionGroup(m_chartRegionSelectionShapeActionGroup);
    ui->actionStartPolygonSelect->setActionGroup(m_chartRegionSelectionShapeActionGroup);
    //ui->actionClearAllSelectiedRegion->setActionGroup(m_chartRegionSelectionShapeActionGroup);
    //ui->actionClearAllSelectiedRegion->setChecked(true);
    //矩形选框
    connect(ui->actionStartRectSelect,&QAction::triggered
            ,this,&MainWindow::onActionStartRectSelectTriggered);
    //椭圆选框
    connect(ui->actionStartEllipseSelect,&QAction::triggered
            ,this,&MainWindow::onActionStartEllipseSelectTriggered);
    //多边形选框
    connect(ui->actionStartPolygonSelect,&QAction::triggered
            ,this,&MainWindow::onActionStartPolygonSelectTriggered);
    //清除所有选区
    connect(ui->actionClearAllSelectiedRegion,&QAction::triggered
            ,this,&MainWindow::onActionClearAllSelectiedRegionTriggered);
    ui->ribbonButtonStartSelection->setDefaultAction(ui->actionStartRectSelect);
    ui->ribbonButtonStartSelection->setChecked(false);
    //选区数据变换
    connect(ui->actionSelectionRegionMove,&QAction::triggered
            ,this,&MainWindow::onActionSelectionRegionMove);
    //
    m_chartRegionSelectionModeActionGroup = new QActionGroup(this);
    ui->actionSingleSelection->setActionGroup(m_chartRegionSelectionModeActionGroup);
    ui->actionAdditionalSelection->setActionGroup(m_chartRegionSelectionModeActionGroup);
    ui->actionSubtractionSelection->setActionGroup(m_chartRegionSelectionModeActionGroup);
    ui->actionIntersectionSelection->setActionGroup(m_chartRegionSelectionModeActionGroup);
    //选区单选模式
    connect(ui->actionSingleSelection,&QAction::triggered
            ,this,&MainWindow::onActionSingleSelectionTriggered);
    //选区多选模式
    connect(ui->actionAdditionalSelection,&QAction::triggered
            ,this,&MainWindow::onActionAdditionalSelectionTriggered);
    //选区减选模式
    connect(ui->actionSubtractionSelection,&QAction::triggered
            ,this,&MainWindow::onActionSubtractionSelectionTriggered);
    //选区交集模式
    connect(ui->actionIntersectionSelection,&QAction::triggered
            ,this,&MainWindow::onActionIntersectionSelectionTriggered);
    //数据显示
    ui->actionYDataPicker->setCheckable(true);
    //拾取y值
    connect(ui->actionYDataPicker,&QAction::triggered,this,&MainWindow::onActionYDataPickerTriggered);
    ui->actionXYDataPicker->setCheckable(true);
    //拾取xy值
    connect(ui->actionXYDataPicker,&QAction::triggered,this,&MainWindow::onActionXYDataPickerTriggered);
    //选区范围内的数据移动
    connect(ui->actionSelectionRegionDataMove,&QAction::triggered,this,&MainWindow::onActionSelectionRegionDataMove);
#ifndef SA_USE_RIBBON_UI
    toolbtn = qobject_cast<QToolButton*>(ui->toolBar_chartSet->widgetForAction(ui->actionXYDataPicker));
    if(toolbtn)
    {
        QMenu* m = new QMenu(toolbtn);
        m->addAction(ui->actionYDataPicker);
        toolbtn->setPopupMode(QToolButton::MenuButtonPopup);
        toolbtn->setMenu(m);
    }
#endif
    //网格
    ui->actionShowGrid->setCheckable(true);
    connect(ui->actionShowGrid,&QAction::triggered,this,&MainWindow::onActionShowGridTriggered);
#ifndef SA_USE_RIBBON_UI
    toolbtn = qobject_cast<QToolButton*>(ui->toolBar_chartSet->widgetForAction(ui->actionShowGrid));
    if(toolbtn)
    {
        QMenu* m1 = new QMenu(toolbtn);
        m1->addAction(ui->actionShowHGrid);
        m1->addAction(ui->actionShowCrowdedHGrid);
        m1->addAction(ui->actionShowVGrid);
        m1->addAction(ui->actionShowCrowdedVGrid);
        toolbtn->setPopupMode(QToolButton::MenuButtonPopup);
        toolbtn->setMenu(m1);
    }
#endif
    //显示水平网格
    ui->actionShowHGrid->setCheckable(true);
    connect(ui->actionShowHGrid,&QAction::triggered,this,&MainWindow::onActionShowHGridTriggered);
    //显示密集水平网格
    ui->actionShowCrowdedHGrid->setCheckable(true);
    connect(ui->actionShowCrowdedHGrid,&QAction::triggered,this,&MainWindow::onActionShowCrowdedHGridTriggered);
    //显示垂直网格
    ui->actionShowVGrid->setCheckable(true);
    connect(ui->actionShowVGrid,&QAction::triggered,this,&MainWindow::onActionShowVGridTriggered);
    //显示密集垂直网格
    ui->actionShowCrowdedVGrid->setCheckable(true);
    connect(ui->actionShowCrowdedVGrid,&QAction::triggered,this,&MainWindow::onActionShowCrowdedVGridTriggered);
    //显示图例
    ui->actionShowLegend->setCheckable(true);
    connect(ui->actionShowLegend,&QAction::triggered,this,&MainWindow::onActionShowLegendTriggered);
    //显示图例选择器
    ui->actionShowLegendPanel->setCheckable(true);
    connect(ui->actionShowLegendPanel,&QAction::triggered,this,&MainWindow::onActionShowLegendPanelTriggered);


    //窗口激活对应数据特性的mdiSubWindowActived
    connect(ui->mdiArea,&QMdiArea::subWindowActivated
            ,ui->dataFeatureWidget,&SADataFeatureWidget::mdiSubWindowActived);
    //数据特性窗口的message显示
    connect(ui->dataFeatureWidget,&SADataFeatureWidget::showMessageInfo
            ,this,&MainWindow::showMessageInfo);
    //窗口关闭的消息在 on_subWindow_close里


    //saValueManager和saUI的关联
    connect(saValueManager,&SAValueManager::messageInformation
            ,this,&MainWindow::showMessageInfo);
    connect(saValueManager,&SAValueManager::dataRemoved
            ,this,&MainWindow::onDataRemoved);
    //SAProjectManager和saUI的关联
    connect(saProjectManager,&SAProjectManager::messageInformation
            ,this,&MainWindow::showMessageInfo);
    //功能性关联
    connect(this,&MainWindow::cleanedProject,ui->tabWidget_valueViewer,&SATabValueViewerWidget::clearAndReleaseAll);
    showMaximized();
    ui->actionWindowMode->setChecked(QMdiArea::SubWindowView == ui->mdiArea->viewMode());
    ui->actionTabMode->setChecked(QMdiArea::TabbedView == ui->mdiArea->viewMode());
}



void MainWindow::initMenu()
{
    //生成
    //变量管理菜单的初始化
    m_menuValueManagerView = new QMenu(ui->treeView_valueManager);
    //在当前标签中打开
    m_menuValueManagerView->addAction(ui->actionViewValueInCurrentTab);
    //在新建标签中打开
    m_menuValueManagerView->addAction(ui->actionViewValueInNewTab);
    m_menuValueManagerView->addSeparator();
    m_menuValueManagerView->addAction(ui->actionRenameValue);//重命名变量
    m_menuValueManagerView->addAction(ui->actionDeleteValue);//删除变量
}

void MainWindow::initPlugin()
{
    m_pluginManager = new SAPluginManager(m_uiInterface,this);
    connect(m_pluginManager,&SAPluginManager::postInfoMessage
            ,this,&MainWindow::showMessageInfo);

    czy::QtApp::QWaitCursor wait;
    int count = m_pluginManager->loadPlugins();
    QString str = tr("load %1 plugins").arg(count);
    showNormalMessageInfo(str);
}

void MainWindow::initTheme()
{
    //皮肤的初始化
    QStringList skinList = SAThemeManager::getSkinList();
    foreach (const QString& sk, skinList) {
        QAction* act = new QAction(sk,this);
        act->setCheckable(true);
        ui->menuSkinList->addAction(act);
        ui->actionGroupSkins->addAction(act);
    }
    connect(ui->actionGroupSkins,&QActionGroup::triggered
            ,this,&MainWindow::onActionSkinChanged);
}

void MainWindow::initUIReflection()
{
    saUIRef->setupUI(m_uiInterface);//saUI保存主窗口指针
    saProjectManager->setupUI(m_uiInterface);
    //初始化项目管理器的其他函数指针
    auto funSaveAction = [this](SAProjectManager* pm){
        if(!pm->isValid())
        {
            return;
        }
        QString folderPath = pm->getProjectSubWindowFolderPath(true);
        __saveSubWindowToFolder(folderPath);
    };
    auto funLoadAction = [this](SAProjectManager* pm){
        if(!pm->isValid())
        {
            return;
        }
        QString folderPath = pm->getProjectSubWindowFolderPath(true);
        __loadSubWindowFromFolder(folderPath);
    };
    saProjectManager->addSaveFunctionAction(funSaveAction);
    saProjectManager->addLoadFunctionAction(funLoadAction);
}
///
/// \brief 打开用户界面支持的其它进程
///
void MainWindow::initProcess()
{
    QString path = qApp->applicationDirPath()+"/signADataProc.exe";
    QStringList args = {QString::number(qApp->applicationPid())};
    QProcess::startDetached(path,args);//signADataProc是一个单例进程，多个软件不会打开多个
}

///
/// \brief 重置布局
///
void MainWindow::onActionSetDefalutDockPosTriggered()
{
    addDockWidget(Qt::LeftDockWidgetArea,ui->dockWidget_valueManage);//从最左上角的dock开始布置，先把列布置完
    splitDockWidget(ui->dockWidget_valueManage,ui->dockWidget_main,Qt::Horizontal);
    splitDockWidget(ui->dockWidget_main,ui->dockWidget_plotLayer,Qt::Horizontal);
    splitDockWidget(ui->dockWidget_valueManage,ui->dockWidget_windowList,Qt::Vertical);
    splitDockWidget(ui->dockWidget_main,ui->dockWidget_chartDataViewer,Qt::Vertical);
    splitDockWidget(ui->dockWidget_plotLayer,ui->dockWidget_DataFeature,Qt::Vertical);
    tabifyDockWidget(ui->dockWidget_main,ui->dockWidget_valueViewer);
    tabifyDockWidget(ui->dockWidget_chartDataViewer,ui->dockWidget_message);
    tabifyDockWidget(ui->dockWidget_windowList,ui->dockWidget_plotSet);
    ui->dockWidget_valueManage->show();
    //ui->dockWidget_valueManage->resize(QSize(500,ui->dockWidget_valueManage->height()));
    ui->dockWidget_plotSet->show();
    ui->dockWidget_windowList->show();
    ui->dockWidget_plotLayer->show();
    ui->dockWidget_chartDataViewer->show();
    ui->dockWidget_DataFeature->show();
    ui->dockWidget_valueViewer->show();
    ui->dockWidget_main->show();
    ui->dockWidget_message->show();
    ui->dockWidget_chartDataViewer->raise();
    ui->dockWidget_main->raise();

}
///
/// \brief 标签模式
/// \param on
///
void MainWindow::onActionTabModeTriggered(bool on)
{
    czy::QtApp::QWaitCursor waitCur;
    Q_UNUSED(waitCur);
    ui->actionWindowMode->setChecked(!on);

    if(on){
        if(QMdiArea::TabbedView != this->ui->mdiArea->viewMode()){
            ui->mdiArea->setViewMode(QMdiArea::TabbedView);
        }
    }else{
        if(QMdiArea::TabbedView == this->ui->mdiArea->viewMode()){
            ui->mdiArea->setViewMode(QMdiArea::SubWindowView);
        }
    }
}
///
/// \brief 窗口模式
/// \param on
///
void MainWindow::onActionWindowModeTriggered(bool on)
{
    czy::QtApp::QWaitCursor waitCur;
    Q_UNUSED(waitCur);
    ui->actionTabMode->setChecked(!on);
    if(on){
        if(QMdiArea::SubWindowView != this->ui->mdiArea->viewMode()){
            ui->mdiArea->setViewMode(QMdiArea::SubWindowView);
        }
    }else{
        if(QMdiArea::SubWindowView == this->ui->mdiArea->viewMode()){
            ui->mdiArea->setViewMode(QMdiArea::TabbedView);
        }
    }
}
///
/// \brief 窗口模式 - 层叠布置
/// \param on
///
void MainWindow::onActionWindowCascadeTriggered(bool on)
{
    Q_UNUSED(on);
    czy::QtApp::QWaitCursor waitCur;
    Q_UNUSED(waitCur);
    if(QMdiArea::SubWindowView == ui->mdiArea->viewMode()){
        ui->mdiArea->cascadeSubWindows();
    }
}
///
/// \brief 窗口模式 - 均匀布置
/// \param on
///
void MainWindow::onActionWindowTileTriggered(bool on)
{
    Q_UNUSED(on);
    czy::QtApp::QWaitCursor waitCur;
    Q_UNUSED(waitCur);
    if(QMdiArea::SubWindowView == ui->mdiArea->viewMode()){
        ui->mdiArea->tileSubWindows();
    }
}
///
/// \brief 显示隐藏DataFeatureDock窗口
/// \param on
///
void MainWindow::onActionDataFeatureDockTriggered(bool on)
{
    Q_UNUSED(on);
    ui->dockWidget_DataFeature->show();
    ui->dockWidget_DataFeature->raise();
}
///
/// \brief 显示隐藏SubWindowListDock窗口
/// \param on
///
void MainWindow::onActionSubWindowListDockTriggered(bool on)
{
    Q_UNUSED(on);
    ui->dockWidget_windowList->show();
    ui->dockWidget_windowList->raise();
}
///
/// \brief 显示隐藏ValueManagerDock窗口
/// \param on
///
void MainWindow::onActionValueManagerDockTriggered(bool on)
{
    Q_UNUSED(on);
    ui->dockWidget_valueManage->show();
    ui->dockWidget_valueManage->raise();
}
///
/// \brief 显示隐藏LayerOutDock窗口
/// \param on
///
void MainWindow::onActionLayerOutDockTriggered(bool on)
{
    Q_UNUSED(on);
    ui->dockWidget_plotLayer->show();
    ui->dockWidget_plotLayer->raise();
}
///
/// \brief 显示隐藏ValueViewerDock窗口
/// \param on
///
void MainWindow::onActionValueViewerDockTriggered(bool on)
{
    Q_UNUSED(on);
    ui->dockWidget_valueViewer->show();
    ui->dockWidget_valueViewer->raise();
}
///
/// \brief 显示隐藏FigureViewer窗口
/// \param on
///
void MainWindow::onActionFigureViewerTriggered(bool on)
{
    Q_UNUSED(on);
    ui->dockWidget_main->show();
    ui->dockWidget_main->raise();
}
///
/// \brief 显示隐藏message窗口
/// \param on
///
void MainWindow::onActionMessageInfoDockTriggered(bool on)
{
    Q_UNUSED(on);
    ui->dockWidget_message->show();
    ui->dockWidget_message->raise();
}

void MainWindow::onActionProjectSettingTriggered()
{
    setProjectInfomation();
}
///
/// \brief 皮肤切换
/// \param on
///
void MainWindow::onActionSkinChanged(QAction* act)
{
    if(nullptr == act)
    {
        return;
    }
    QString name = act->text();
    setSkin(name);
}

void MainWindow::setSkin(const QString &name)
{
    saStartElapsed(tr("start use skin:%1").arg(name));
    SAThemeManager::setStyle(name,this);
    QList<QAction*> acts = ui->actionGroupSkins->actions();
    for(int i=0;i<acts.size();++i)
    {
        if(acts[i]->text() == name)
        {
            acts[i]->setChecked(true);
            break;
        }
    }
    saElapsed(tr("end use skin"));
}

SAUIInterface *MainWindow::uiInterface()
{
    return m_uiInterface;
}




MainWindow::~MainWindow()
{
    saveSetting();
#ifndef SA_USE_RIBBON_UI
    delete ui;
#endif
}

void MainWindow::loadSetting()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "CZY", "SA");
	loadWindowState(settings);
}

void MainWindow::saveSetting()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "CZY", "SA");
	//mainWindow
	saveWindowState(settings);
	
}

void MainWindow::saveWindowState(QSettings& setting)
{
    setting.beginGroup("StartTimes");
    setting.setValue("firstStart", false);
    setting.endGroup();

	setting.beginGroup("mainWindow");
	setting.setValue("geometry", saveGeometry());
	setting.setValue("windowState", saveState());
	setting.endGroup();

    setting.beginGroup("skin");
    setting.setValue("name",SAThemeManager::currentStyleName() );
    setting.endGroup();

}
void MainWindow::loadWindowState(const QSettings& setting)
{
	QVariant var = QVariant();
    var = setting.value("StartTimes/firstStart");
    if(var.isValid())
    {
        if(var.toBool())
            onActionSetDefalutDockPosTriggered();
    }
    else
    {
        onActionSetDefalutDockPosTriggered();
    }
	var = setting.value("mainWindow/geometry");
	if(var.isValid())
		restoreGeometry(var.toByteArray());
	var = setting.value("mainWindow/windowState");
	if(var.isValid())
		restoreState(var.toByteArray());

    var = setting.value("skin/name");
    if(var.isValid())
        setSkin(var.toString());
    else
        setSkin("normal");
}

void MainWindow::releaseChart2DEditor(SAChart2D* chart)
{
    if(chart)
    {
        if(chart->getEditor())
        {
            chart->setEditor(nullptr);
        }
    }
    ui->actionSelectionRegionMove->setChecked(false);
    ui->actionSelectionRegionDataMove->setChecked(false);
}




///
/// \brief 曲线数据发生变动
/// NOTE: 此函数还未完成，需要注意
/// \param widget
/// \param pC
/// TODO:
void MainWindow::onChartDataChanged(QWidget *widget,const QwtPlotItem *pC)
{
    Q_UNUSED(pC)
    Q_UNUSED(widget)
    //TODO

}


///
/// \brief 变量管理树形视图的单击触发
/// \param index
///
void MainWindow::onTreeViewValueManagerClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    //把选中的数据传递给saUI
    SAAbstractDatas* data = getSeletedData();
    if(data)
    {
        m_uiInterface->onSelectDataChanged(data);
        emit selectDataChanged(data);
    }
}
///
/// \brief 变量管理树形视图的双击触发
/// \param index
///
void MainWindow::onTreeViewValueManagerDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    QList<SAAbstractDatas*> datas=getSeletedDatas();
    if(datas.size ()<=0)
        return;
    ui->tabWidget_valueViewer->setDataInNewTab(datas);
    if (ui->dockWidget_valueViewer->isHidden())
        ui->dockWidget_valueViewer->show();
    ui->dockWidget_valueViewer->raise();
}
///
/// \brief 变量管理树形视图的右键触发
/// \param pos
///
void MainWindow::onTreeViewValueManagerCustomContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);
    if(m_menuValueManagerView)
        m_menuValueManagerView->exec (QCursor::pos());
}

SAValueManagerModel*MainWindow::getValueManagerModel() const
{
    return ui->treeView_valueManager->getValueManagerModel();
}


SADrawDelegate*MainWindow::getDrawDelegate() const
{
    return m_drawDelegate.data();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        saPrint() << "dragEnterEvent SAValueManagerMimeData::valueIDMimeType()";
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        saPrint() << "dragMoveEvent SAValueManagerMimeData::valueIDMimeType()";
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        QList<int> ids;
        if(SAValueManagerMimeData::getValueIDsFromMimeData(event->mimeData(),ids))
        {
            saPrint() << "dropEvent SAValueManagerMimeData::valueIDMimeType()";
            QList<SAAbstractDatas*> datas = saValueManager->findDatas(ids);
            if(datas.size() > 0)
            {
                m_drawDelegate->drawLine(datas);
            }
        }
    }
}

///
/// \brief 打开action
///
void MainWindow::onActionOpenTriggered()
{
    QFileDialog openDlg(this);
    QStringList strNFilter = m_pluginManager->getOpenFileNameFilters();

    strNFilter.push_back(tr("all files (*.*)"));
    openDlg.setFileMode(QFileDialog::ExistingFile);
    openDlg.setNameFilters(strNFilter);
    if (QDialog::Accepted == openDlg.exec())
    {
        QStringList strfileNames = openDlg.selectedFiles();
        if(strfileNames.isEmpty())
            return;

        QString strFile = strfileNames.value(0);
        QFileInfo temDir(strFile);
        QString suffix = temDir.suffix();
        suffix = suffix.toLower();
        SAAbstractDataImportPlugin* import = m_pluginManager->getDataImportPluginFromSuffix(suffix);
        if(nullptr != import)
        {
            import->openFile(strfileNames);
        }
    }
}

///
/// \brief 打开项目文件夹
///
void MainWindow::onActionOpenProjectTriggered()
{
    QString path = QFileDialog::getExistingDirectory(this,QStringLiteral("选择项目目录"));
    if(path.isEmpty())
        return;
    if(saProjectManager->load(path))
    {
        raiseValueManageDock();
    }
}


///
/// \brief 保存
///
void MainWindow::onActionSaveTriggered()
{
    QString projectPath = saProjectManager->getProjectFullPath();
    if(projectPath.isEmpty())
    {
        if(saProjectManager->getProjectName().isEmpty())
        {
            if(!setProjectInfomation())
            {
                showWarningMessageInfo(tr("you need to set a project name"));
                return;
            }
        }
        onActionSaveAsTriggered();
    }
    else
    {
        saProjectManager->save();
    }
}
///
/// \brief 另存为
///
void MainWindow::onActionSaveAsTriggered()
{
    QString path = QFileDialog::getSaveFileName(this,QStringLiteral("保存"),QString()
                                 ,QString(),0,QFileDialog::ShowDirsOnly);
    if(path.isEmpty())
        return;
    saProjectManager->saveAs(path);
}


///
/// \brief 添加新图例
///
void MainWindow::onActionNewChartTriggered()
{
    Dialog_AddChart addChart(this);

    if(QDialog::Accepted == addChart.exec())
    {
        SAMdiSubWindow* pSubWnd = m_drawDelegate->createFigureMdiSubWidget();
        SAFigureWindow* pFigWnd = getFigureWidgetFromMdiSubWindow (pSubWnd);
        if(nullptr == pFigWnd)
        {
            return;
        }
        SAChart2D* pC = pFigWnd->create2DPlot();
        pC->setAutoReplot(false);
        QList<QwtPlotCurve*> curList = addChart.getDrawCurveList();
        for(auto ite = curList.begin();ite != curList.end();++ite)
        {
            (*ite)->detach();//先要和原来的脱离连接才能绑定到新图
            pC->addItem(*ite);
        }
        bool isDateTime = false;
        QString tf = addChart.isAxisDateTime(&isDateTime,QwtPlot::xBottom);
        if(isDateTime)
            pC->setAxisDateTimeScale(tf,QwtPlot::xBottom);
        isDateTime = false;
        tf = addChart.isAxisDateTime(&isDateTime,QwtPlot::yLeft);
        if(isDateTime)
            pC->setAxisDateTimeScale(tf,QwtPlot::yLeft);
        pC->enableZoomer(false);
        pC->enablePicker(false);
        pC->enableGrid(true);
        pC->setAxisTitle(QwtPlot::xBottom,addChart.chart()->axisTitle(QwtPlot::xBottom).text());
        pC->setAxisTitle(QwtPlot::yLeft,addChart.chart()->axisTitle(QwtPlot::yLeft).text());
        pC->enableGrid(true);
        pC->setAutoReplot(true);
        pSubWnd->show();
    }
}
///
/// \brief 绘制线图
///
void MainWindow::onActionAddLineChartTriggered()
{
    raiseMainDock();
    QList<SAAbstractDatas*> datas = getSeletedDatas();
    if(datas.size() != 0)
    {
        QList<QwtPlotCurve *> res = m_drawDelegate->drawLine(datas);
    }
}
///
/// \brief 绘制棒图
///
void MainWindow::onActionAddBarChartTriggered()
{
    raiseMainDock();
    QList<SAAbstractDatas*> datas = getSeletedDatas();
    if(datas.size() != 0)
    {
        QList<QwtPlotBarChart *> res = m_drawDelegate->drawBar(datas);
    }
}

void MainWindow::onActionAddHistogramChartTriggered()
{
    raiseMainDock();
    QList<SAAbstractDatas*> datas = getSeletedDatas();
    if(datas.size() != 0)
    {
        QList<QwtPlotHistogram *> res = m_drawDelegate->drawHistogram(datas);
    }
}
///
/// \brief 绘制散点图
///
void MainWindow::onActionAddScatterChartTriggered()
{
    raiseMainDock();
    QList<SAAbstractDatas*> datas = getSeletedDatas();
    if(datas.size() != 0)
    {
        QList<QwtPlotCurve *> res = m_drawDelegate->drawScatter(datas);
    }
}
///
/// \brief  绘制Box图
///
void MainWindow::onActionAddBoxChartTriggered()
{
    raiseMainDock();
    QList<SAAbstractDatas*> datas = getSeletedDatas();
    if(datas.size() != 0)
    {
        QList<QwtPlotCurve *> res = m_drawDelegate->drawScatter(datas);
    }
}

///
/// \brief 图表开始矩形选框工具
/// \param b
///
void MainWindow::onActionStartRectSelectTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        releaseChart2DEditor(chart);
        if(b)
        {
            chart->unenableEditor();
            chart->enableSelection(SAChart2D::RectSelection,b);
            SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor();
            if(selectEditor)
            {
                selectEditor->setSelectionMode(getCurrentChartRegionSelectionMode());
            }
        }
        else
        {
            chart->enableSelection(SAChart2D::RectSelection,false);
        }
    }
    updateChartSetToolBar();


}
///
/// \brief 开始圆形选框工具
/// \param b
///
void MainWindow::onActionStartEllipseSelectTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();

    if(chart)
    {
        releaseChart2DEditor(chart);
        if(b)
        {
            chart->unenableEditor();
            chart->enableSelection(SAChart2D::EllipseSelection,b);
            SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor();
            if(selectEditor)
            {
                selectEditor->setSelectionMode(getCurrentChartRegionSelectionMode());
            }
        }
        else
        {
            chart->enableSelection(SAChart2D::EllipseSelection,false);
        }
    }
    updateChartSetToolBar();
}
///
/// \brief 开始多边形选框工具
/// \param b
///
void MainWindow::onActionStartPolygonSelectTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        releaseChart2DEditor(chart);
        if(b)
        {
            chart->unenableEditor();
            chart->enableSelection(SAChart2D::PolygonSelection,b);
            SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor();
            if(selectEditor)
            {
                selectEditor->setSelectionMode(getCurrentChartRegionSelectionMode());
            }
        }
        else
        {
            chart->enableSelection(SAChart2D::PolygonSelection,false);
        }
    }
    updateChartSetToolBar();
}

///
/// \brief 清除所有选区
/// \param b
///
void MainWindow::onActionClearAllSelectiedRegionTriggered(bool b)
{
    Q_UNUSED(b);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        chart->clearAllSelectedRegion();
        releaseChart2DEditor(chart);
    }
    updateChartSetToolBar();
}
///
/// \brief 选区单选模式
/// \param b
///
void MainWindow::onActionSingleSelectionTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        if(b)
        {
            SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor();
            if(selectEditor)
            {
                selectEditor->setSelectionMode(SAAbstractRegionSelectEditor::SingleSelection);
            }
        }
    }
}
///
/// \brief 选区多选模式
/// \param b
///
void MainWindow::onActionAdditionalSelectionTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        if(b)
        {
            SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor();
            if(selectEditor)
            {
                selectEditor->setSelectionMode(SAAbstractRegionSelectEditor::AdditionalSelection);
            }
        }
    }
}
///
/// \brief 选区减选模式
/// \param b
///
void MainWindow::onActionSubtractionSelectionTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        if(b)
        {
            SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor();
            if(selectEditor)
            {
                selectEditor->setSelectionMode(SAAbstractRegionSelectEditor::SubtractionSelection);
            }
        }
    }
}
///
/// \brief 选区交集模式
/// \param b
///
void MainWindow::onActionIntersectionSelectionTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        if(b)
        {
            SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor();
            if(selectEditor)
            {
                selectEditor->setSelectionMode(SAAbstractRegionSelectEditor::IntersectionSelection);
            }
        }
    }
}
///
/// \brief 选区移动
/// \param b
///
void MainWindow::onActionSelectionRegionMove(bool b)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(nullptr == chart)
    {
        ui->actionSelectionRegionMove->setChecked(false);
        return;
    }

    if(b)
    {
        if(SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor())
        {
            saAddLog("Selection Region Move");
            raiseMainDock();
            chart->unenableEditor();
            selectEditor->setEnabled(false);
            SASelectRegionEditor* editor = new SASelectRegionEditor(chart);
            editor->setObjectName(QStringLiteral("SASelectRegionEditor"));
            chart->setEditor(editor);

        }
        else
        {
            ui->actionSelectionRegionMove->setChecked(false);
            return;
        }
    }
    else
    {
        chart->setEditor(nullptr);
    }
    updateSelectActionState(chart);
    updateChartEditorActionState(chart);
}
///
/// \brief 选区范围内的数据移动
/// \param on
///
void MainWindow::onActionSelectionRegionDataMove(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(nullptr == chart)
    {
        ui->actionSelectionRegionDataMove->setChecked(false);
        return;
    }
    if(on)
    {
        if(SAAbstractRegionSelectEditor* selectEditor = chart->getRegionSelectEditor())
        {
            saAddLog("Selection Region Datas Move");
            if(!chart->isCurrentSelectItemsHaveChartItem())
            {
                QList<QwtPlotItem*> selCur = CurveSelectDialog::getSelectChartPlotItems(chart,this);
                chart->setCurrentSelectItems(selCur);
            }
            raiseMainDock();
            if(chart->canvas())
            {
                chart->canvas()->setFocus();
            }
            selectEditor->setEnabled(false);
            chart->unenableEditor();
            SASelectRegionDataEditor* editor = new SASelectRegionDataEditor(chart);
            editor->setObjectName(QStringLiteral("SASelectRegionDataEditor"));
            chart->setEditor(editor);
        }
        else
        {
            ui->actionSelectionRegionMove->setChecked(false);
            return;
        }
    }
    updateSelectActionState(chart);
    updateChartEditorActionState(chart);
}

///
/// \brief 获取当前ui选择的区域选择模式
/// \return
///
SAAbstractRegionSelectEditor::SelectionMode MainWindow::getCurrentChartRegionSelectionMode() const
{
    QAction* act = m_chartRegionSelectionModeActionGroup->checkedAction();
    if(act == ui->actionSingleSelection)
    {
        return SAAbstractRegionSelectEditor::SingleSelection;
    }
    else if(act == ui->actionAdditionalSelection)
    {
        return SAAbstractRegionSelectEditor::AdditionalSelection;
    }
    else if(act == ui->actionSubtractionSelection)
    {
        return SAAbstractRegionSelectEditor::SubtractionSelection;
    }
    else if(act == ui->actionIntersectionSelection)
    {
        return SAAbstractRegionSelectEditor::IntersectionSelection;
    }
    return SAAbstractRegionSelectEditor::SingleSelection;
}

///
/// \brief 开启当前绘图的十字光标
///
void MainWindow::onActionEnableChartPickerTriggered(bool check)
{
    SAFigureWindow* fig = getCurrentFigureWindow();
    if(fig)
    {
        QList<SAChart2D*> charts = fig->get2DPlots();
        std::for_each(charts.begin(),charts.end(),[check](SAChart2D* c){
            c->unenableEditor();
            c->enablePicker(check);
        });
        releaseChart2DEditor(fig->current2DPlot());
    }
    updateChartSetToolBar(fig);
}
///
/// \brief 开启当前绘图的拖动
/// \param check
///
void MainWindow::onActionEnableChartPannerTriggered(bool check)
{
    SAFigureWindow* fig = getCurrentFigureWindow();
    if(fig)
    {
        QList<SAChart2D*> charts = fig->get2DPlots();
        std::for_each(charts.begin(),charts.end(),[check](SAChart2D* c){
            c->unenableEditor();
            c->enablePanner(check);
        });
        releaseChart2DEditor(fig->current2DPlot());
    }
    updateChartSetToolBar(fig);
}
///
/// \brief 开启当前绘图的区间缩放
/// \param check
///
void MainWindow::onActionEnableChartZoomTriggered(bool check)
{
    SAFigureWindow* fig = getCurrentFigureWindow();
    if(fig)
    {
        QList<SAChart2D*> charts = fig->get2DPlots();
        std::for_each(charts.begin(),charts.end(),[check](SAChart2D* c){
            c->unenableEditor();
            c->enableZoomer(check);
        });
        releaseChart2DEditor(fig->current2DPlot());
    }
    updateChartSetToolBar(fig);
}
///
/// \brief 当前绘图的缩放还原
/// \param check
///
void MainWindow::onActionSetChartZoomToBaseTriggered(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
        chart->setZoomBase();
}
///
/// \brief 当前绘图放大
/// \param check
///
void MainWindow::onActionChartZoomInTriggered(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        bool isEnableZoomer = chart->isEnableZoomer();
        chart->zoomIn();
        chart->enableZoomer(isEnableZoomer);
    }
}
///
/// \brief 当前绘图缩小
/// \param check
///
void MainWindow::onActionChartZoomOutTriggered(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        bool isEnableZoomer = chart->isEnableZoomer();
        chart->zoomOut();
        chart->enableZoomer(isEnableZoomer);
    }
}
///
/// \brief 当前绘图重置
/// \param check
///
void MainWindow::onActionChartZoomResetTriggered(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
        chart->setZoomReset();
}
///
/// \brief 拾取y值
/// \param on
///
void MainWindow::onActionYDataPickerTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->unenableEditor();
        chart->enableYDataPicker(on);
    }
    releaseChart2DEditor(chart);
    updateChartSetToolBar();
}
///
/// \brief 拾取xy值
/// \param on
///
void MainWindow::onActionXYDataPickerTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->unenableEditor();
        chart->enableXYDataPicker(on);
    }
    releaseChart2DEditor(chart);
    updateChartSetToolBar();
}
///
/// \brief 网格
/// \param on
///
void MainWindow::onActionShowGridTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->enableGrid(on);
    }
    updateChartGridActionState(chart);
}
///
/// \brief 显示水平网格
/// \param on
///
void MainWindow::onActionShowHGridTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->enableGridY(on);
    }
    updateChartGridActionState(chart);
}
///
/// \brief 垂直网格
/// \param on
///
void MainWindow::onActionShowVGridTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->enableGridX(on);
    }
    updateChartGridActionState(chart);
}
///
/// \brief 显示密集水平网格
/// \param on
///
void MainWindow::onActionShowCrowdedHGridTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->enableGridYMin(on);
    }
    updateChartGridActionState(chart);
}
///
/// \brief 显示密集垂直网格
/// \param on
///
void MainWindow::onActionShowCrowdedVGridTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->enableGridXMin(on);
    }
    updateChartGridActionState(chart);
}
///
/// \brief 显示图例
/// \param on
///
void MainWindow::onActionShowLegendTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->enableLegend(on);
    }
    updateChartSetToolBar();
}
///
/// \brief 显示图例选择器
/// \param on
///
void MainWindow::onActionShowLegendPanelTriggered(bool on)
{
    SAChart2D* chart = getCurSubWindowChart();
    if(chart)
    {
        chart->enableLegendPanel(on);
    }
    updateChartSetToolBar();
}



///
/// \brief 清除项目
///
void MainWindow::onActionClearProjectTriggered()
{
    if(QMessageBox::No == QMessageBox::question(this
                                                ,QStringLiteral("通知")
                                                ,QStringLiteral("确认清除项目？\n清楚项目将会把当前所有操作清空")
                                                ))
    {
        return;
    }
    emit cleaningProject();
    saValueManager->clear();
    QList<QMdiSubWindow*> subWindows = ui->mdiArea->subWindowList();
    for (auto ite = subWindows.begin();ite != subWindows.end();++ite)
    {
        SAMdiSubWindow* wnd = qobject_cast<SAMdiSubWindow*>((*ite));
        wnd->askOnCloseEvent(false);
    }
    ui->mdiArea->closeAllSubWindows();
    showNormalMessageInfo(QStringLiteral("清除方案"),0);
    updateChartSetToolBar();
    //数据预览窗口清空

    emit cleanedProject();
}





///
/// \brief 子窗口激活
/// \param arg1
///
void MainWindow::onMdiAreaSubWindowActivated(QMdiSubWindow *arg1)
{

    if(nullptr == arg1)
        return;
//    if(m_lastActiveWnd == arg1)
//        return;
//    m_lastActiveWnd = arg1;

    SAFigureWindow* fig = getFigureWidgetFromMdiSubWindow(arg1);
    if(fig)
    {
#ifdef SA_USE_RIBBON_UI

#else
      ui->toolBar_chartSet->setEnabled(true);
#endif
        //窗口激活后，把绘图窗口的指针保存
        m_lastShowFigureWindow = fig;


        //刷新toolbar
        updateChartSetToolBar(fig);

        //更新dock - plotLayer 图层
		SAPlotLayerModel* plotLayerModel = getPlotLayerModel();
		if(plotLayerModel)
		{
            SAChart2D* plot = fig->current2DPlot();
            if(plot)
            {
                plotLayerModel->setPlot(plot);
                QItemSelectionModel* selectModel = ui->tableView_layer->selectionModel();
                QList<QwtPlotItem*> selItems = plot->getCurrentSelectItems();
                if(selectModel)
                {
                   QModelIndexList indexSel = plotLayerModel->getIndexFromPlotItems(selItems);
                   selectModel->reset();
                   for(int i=0;i<indexSel.size();++i)
                   {
                       //qDebug() << "indexSel r:"<<indexSel[i].row()<<" c:"<<indexSel[i].column();
                       selectModel->select(indexSel[i],QItemSelectionModel::Select|QItemSelectionModel::Rows);
                   }
                }
            }
		}
        QList<SAChart2D*> plotWidgets = getCurSubWindowCharts();
        QList<QwtPlot*> qwtChart;
        std::for_each(plotWidgets.begin(),plotWidgets.end(),[&](SAChart2D* p){
            qwtChart.append(static_cast<QwtPlot*>(p));
        });
        //更新dock - dataviewer
        QwtPlotItemTreeModel* qwtItemTreeModel = getDataViewerPlotItemTreeModel();
        if(qwtItemTreeModel)
        {

            qwtItemTreeModel->setPlots(qwtChart);
            ui->treeView_curPlotItem->expandAll();
        }
        //新窗口激活后，把原来显示的数据clear
        QwtPlotItemDataModel* plotItemDataModel = getDataViewerPlotItemDataModel();
        if(plotItemDataModel)
        {
            plotItemDataModel->clear();
        }
    }
    else
    {
    }
    //设置绘图属性窗口,空指针也接受
    ui->figureSetWidget->setFigureWidget(fig);
}

void MainWindow::onSubWindowClosed(QMdiSubWindow *arg1)
{
    SAFigureWindow* fig = getFigureWidgetFromMdiSubWindow(arg1);
    if(fig)
    {
        //绘图窗口关闭，判断当前记录的绘图窗口指针，若是，就删除
        if(m_lastShowFigureWindow == fig)
        {
            m_lastShowFigureWindow = nullptr;
        }
        //
        getPlotLayerModel()->setPlot(nullptr);
        //窗口关闭,更新dock - dataviewer
        QwtPlotItemTreeModel* qwtItemTreeModel = getDataViewerPlotItemTreeModel();
        if(qwtItemTreeModel)
        {
            qwtItemTreeModel->clear();
        }
        //窗口关闭，把原来显示的数据clear
        QwtPlotItemDataModel* plotItemDataModel = getDataViewerPlotItemDataModel();
        if(plotItemDataModel)
        {
            plotItemDataModel->clear();
        }
    }
    ui->dataFeatureWidget->mdiSubWindowClosed(arg1);
}








///
/// \brief 数据剔除，会把选定区域的数据剔除
///
void MainWindow::onActionInRangDataRemoveTriggered()
{
    SAFigureWindow* wnd = getCurrentFigureWindow();
    if(nullptr == wnd)
    {
        showWarningMessageInfo(tr("you should select a figure window"));
        return;
    }
    SAChart2D* chart = wnd->current2DPlot();
    if(nullptr == chart)
    {
        showWarningMessageInfo(tr("I can not find any chart in figure!"));
        return;
    }

    if(!chart->isCurrentSelectItemsHaveChartItem())
    {
        QList<QwtPlotItem*> selItems = CurveSelectDialog::getSelectChartPlotItems(chart,this);
        qDebug() << selItems.size();
        chart->setCurrentSelectItems(selItems);
        chart->removeDataInRang();
    }
    else
    {
        chart->removeDataInRang();
    }
}


///
/// \brief 隐藏状态栏的进度
///
void MainWindow::hideProgressStatusBar()
{
    ui_status_progress->setVisible(false);
}

///
/// \brief 显示状态栏的进度
///
void MainWindow::showProgressStatusBar()
{
    ui_status_progress->setVisible(true);
}

///
/// \brief 设置状态栏上的进度显示栏是否显示
/// \param isShow
///
void MainWindow::setProgressStatusBarVisible(bool isShow)
{
    ui_status_progress->setVisible(isShow);
}
///
/// \brief 设置状态栏上的进度显示的进度状的百分比
/// \param present
///
void MainWindow::setProgressStatusBarPresent(int present)
{
    ui_status_progress->setPresent(present);
}
///
/// \brief 设置状态栏上的文字
/// \param text
///
void MainWindow::setProgressStatusBarText(const QString &text)
{
    ui_status_progress->setText(text);
}
///
/// \brief 获取进度栏上的进度条指针
/// \return
///
QProgressBar *MainWindow::getProgressStatusBar()
{
    return ui_status_progress->getProgressBar();
}
///
/// \brief 创建一个绘图窗口
/// \note 注意，若没有显示，getCurrentFigureWindow返回的还是上次显示的figureWidget
/// \return 绘图窗口Pointer,如果内存溢出返回nullptr
///
QMdiSubWindow *MainWindow::createFigureWindow(const QString &title)
{
    SAMdiSubWindow* sub = m_drawDelegate->createFigureMdiSubWidget(title);
    return sub;
}

///
/// \brief 获取当前正在显示的图形窗口
/// \return 如果没有或不是显示图形窗口，则返回nullptr
///
SAFigureWindow *MainWindow::getCurrentFigureWindow()
{
    return m_lastShowFigureWindow;
}
///
/// \brief 获取所有的figure
/// \return
///
QList<SAFigureWindow *> MainWindow::getFigureWindowList() const
{
    QList<SAFigureWindow *> res;
    QList<QMdiSubWindow*> subList = ui->mdiArea->subWindowList();
    SAFigureWindow * fig = nullptr;
    for(int i=0;i<subList.size();++i)
    {
        fig = qobject_cast<SAFigureWindow *>(subList[i]->widget());
        if(fig)
        {
            res.append(fig);
        }
    }
    return res;
}
///
/// \brief 获取当前正在显示的Chart指针
/// \return 如果没有或不是显示chart，则返回nullptr
///
SAChart2D *MainWindow::getCurSubWindowChart()
{
    SAFigureWindow* pBase = getCurrentFigureWindow();
    if (nullptr == pBase)
        return nullptr;
    return pBase->current2DPlot();
}
///
/// \brief 获取当前正在显示的Chart指针,如果是一个subplot，返回多个指针
/// \param 如果没有或不是显示chart，则返回nullptr
/// \return
///
QList<SAChart2D*> MainWindow::getCurSubWindowCharts()
{
    SAFigureWindow* pFig = getCurrentFigureWindow();
    if (nullptr == pFig)
        return QList<SAChart2D *>();
    return pFig->get2DPlots();
}
///
/// \brief 用于子窗口激活时刷新“图表设置工具栏的选中状态”
///
void MainWindow::updateChartSetToolBar(SAFigureWindow *w)
{
    if(nullptr == w)
    {
        w = this->getCurrentFigureWindow();
    }
    if(nullptr == w)
    {
        ui->actionEnableChartCrossCursor->setChecked( false );
        ui->actionEnableChartPanner->setChecked( false );
        ui->actionEnableChartZoom->setChecked(false);
        ui->actionYDataPicker->setChecked(false);
        ui->actionXYDataPicker->setChecked(false);
        ui->actionShowLegend->setChecked(false);
        ui->actionShowLegendPanel->setChecked(false);
        updateChartGridActionState(nullptr);
        updateSelectActionState(nullptr);
        updateChartEditorActionState(nullptr);
        return;
    }
    auto c = w->current2DPlot();
    if(c)
    {
        ui->actionEnableChartCrossCursor->setChecked( c->isEnablePicker() );
        ui->actionEnableChartPanner->setChecked( c->isEnablePanner() );
        ui->actionEnableChartZoom->setChecked(c->isEnableZoomer());
        ui->actionYDataPicker->setChecked(c->isEnableYDataPicker());
        ui->actionXYDataPicker->setChecked(c->isEnableXYDataPicker());
        ui->actionShowLegend->setChecked(c->isEnableLegend());
        ui->actionShowLegendPanel->setChecked(c->isEnableLegendPanel());
    }
    updateChartGridActionState(c);
    updateSelectActionState(c);
    updateChartEditorActionState(c);
}

QList<QMdiSubWindow *> MainWindow::getSubWindowList() const
{
    return ui->mdiArea->subWindowList();
}
///
/// \brief 从subwindow指针中查找是否含有SAFigureWindow
/// \param sub subwindow指针
/// \return 存在返回指针，否则返回nullptr
///
SAFigureWindow*MainWindow::getFigureWidgetFromMdiSubWindow(QMdiSubWindow* sub)
{
    return qobject_cast<SAFigureWindow*>(sub->widget());
}
///
/// \brief 记录最后获取焦点的窗口类型，此函数主要用于函数功能模块判断是对图进行操作还是对数据进行操作
/// \return
///
SAUIInterface::LastFocusType MainWindow::lastFocusWidgetType() const
{
    return static_cast<SAUIInterface::LastFocusType>(m_lastForceType);
}
///
/// \brief 设置多文档激活的窗口和QMdiArea::setActiveSubWindow一样
/// \param window
///
void MainWindow::setActiveSubWindow(QMdiSubWindow *window)
{
    ui->mdiArea->setActiveSubWindow(window);
}

///
/// \brief 获取当前激活的子窗口
/// \return 如果窗口没有激活，返回nullptr
///
QMdiSubWindow *MainWindow::getCurrentActiveSubWindow() const
{
    return qobject_cast<SAMdiSubWindow*>(ui->mdiArea->activeSubWindow());
}
///
/// \brief 判断mdi中是否存在指定的子窗口
/// \param
/// \return
///
bool MainWindow::isHaveSubWnd(QMdiSubWindow* wndToCheck) const
{
    if (nullptr == wndToCheck)
        return false;
    QList<QMdiSubWindow*> subs = ui->mdiArea->subWindowList();
    for (auto ite = subs.begin();ite != subs.end();++ite)
    {
        if (*ite == wndToCheck)
        {
            return true;
        }
    }
    return false;
}
///
/// \brief 把主dock抬起，主dock包括绘图的窗口
///
void MainWindow::raiseMainDock()
{
    ui->dockWidget_main->raise();
    ui->mdiArea->setFocus();
}
///
/// \brief 把信息窗口抬起
///
void MainWindow::raiseMessageInfoDock()
{
    ui->dockWidget_message->raise();
}
///
/// \brief 让ValueViewerDock显示到最前面
///
void MainWindow::raiseValueViewerDock()
{
    ui->dockWidget_valueViewer->raise();
}
///
/// \brief 让DataFeatureDock显示到最前面
///
void MainWindow::raiseDataFeatureDock()
{
    ui->dockWidget_DataFeature->raise();
}
///
/// \brief 让windowList显示到最前面
///
void MainWindow::raiseWindowListDock()
{
    ui->dockWidget_windowList->raise();
}
///
/// \brief 让valueManageDock显示到最前面
///
void MainWindow::raiseValueManageDock()
{
    ui->dockWidget_valueManage->raise();
}
///
/// \brief 让ChartLayoutDock显示到最前面
///
void MainWindow::raiseChartLayoutDock()
{
    ui->dockWidget_plotLayer->raise();
}
///
/// \brief 让chartDataViewerDock显示到最前面
///
void MainWindow::raiseChartDataViewerDock()
{
    ui->dockWidget_chartDataViewer->raise();
}
///
/// \brief 让ChartSettingDock显示到最前面
///
void MainWindow::raiseChartSettingDock()
{
    ui->dockWidget_plotSet->raise();
}
///
/// \brief 如果插件只导入到data import菜单下的条目可以调用此函数，如果需要插入一个QMenu可以使用addDataImportPluginAction
/// \param action
///
void MainWindow::addDataImportPluginAction(QAction *action)
{
    ui->menuImport->addAction(action);
}
///
/// \brief 添加导入数据插件菜单
/// \param menu
/// \return
///
QAction *MainWindow::addDataImportPluginMenu(QMenu *menu)
{
    return ui->menuImport->addMenu(menu);
}
///
/// \brief 把菜单添加到分析功能的菜单中
/// \param menu
/// \return
///
QAction *MainWindow::addAnalysisPluginMenu(QMenu *menu)
{
    return ui->menuAnalysis->addMenu(menu);
}
#ifdef SA_USE_RIBBON_UI
///
/// \brief 把action加入 ribbon界面的Gallery
/// \param name
/// \param actions
///
void MainWindow::addAnalysisActionsToRibbonGallery(const QString &name, const QList<QAction *> &actions)
{
    SARibbonGalleryGroup* group = ui->ribbonGalleryFuntions->addCategoryActions(name,actions);
    group->setEnableIconText(true);
}
#endif

///
/// \brief 在ui界面显示普通信息
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showErrorInfo showWarningInfo showQuestionInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showNormalMessageInfo(const QString& info, int interval)
{
    ui->saMessageWidget->addStringWithTime(info,Qt::black);
    if(interval > 0)
        ui_status_info->showNormalInfo (info,interval);
}

///
/// \brief 错误信息警告
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showNormalInfo showWarningInfo showQuestionInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showErrorMessageInfo(const QString& info, int interval)
{
    ui->saMessageWidget->addStringWithTime(info,Qt::red);
    if(interval > 0)
        ui_status_info->showErrorInfo (info,interval);
}
///
/// \brief 在ui界面显示警告信息
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showNormalInfo showErrorInfo showQuestionInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showWarningMessageInfo(const QString& info, int interval)
{
    QColor clr(211,129,10);
    ui->saMessageWidget->addStringWithTime(info,clr);
    if(interval > 0)
        ui_status_info->showWarningInfo (info,interval);
}
///
/// \brief 在ui界面显示询问信息
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showNormalInfo showErrorInfo showWarningInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showQuestionMessageInfo(const QString& info, int interval)
{
    ui->saMessageWidget->addStringWithTime(info,Qt::blue);
    if(interval > 0)
        ui_status_info->showQuesstionInfo (info,interval);
}
///
/// \brief 把消息显示到窗口中
/// \param info 消息
/// \param messageType 需要显示的消息类型 \sa SA::MeaasgeType
/// \see showNormalInfo showErrorInfo showWarningInfo showQuestionInfo showWidgetMessageInfo
///
void MainWindow::showMessageInfo(const QString &info, SA::MeaasgeType messageType)
{
    showWidgetMessageInfo(info,nullptr,messageType,-1);
}
///
/// \brief 接收SAWidget发出的信息
/// \param info 信息内容
/// \param widget 窗口指针,如果不是窗口发送，指定nullptr即可
/// \param messageType 信息类型 \sa SA::MeaasgeType
/// \param interval 信息显示时间
/// \see showNormalInfo showErrorInfo showWarningInfo showQuestionInfo showMessageInfo
///
void MainWindow::showWidgetMessageInfo(const QString& info, QWidget* widget, SA::MeaasgeType messageType, int interval)
{
    if(nullptr != widget)
    {
        ui->saMessageWidget->addString(widget->windowTitle ());
    }
    switch(messageType)
    {
    case SA::NormalMessage:
    {
        showNormalMessageInfo(info,interval);
        break;
    }
    case SA::ErrorMessage:
    {
        showErrorMessageInfo(info,interval);
        break;
    }
    case SA::QuesstionMessage:
    {
        showQuestionMessageInfo(info,interval);
        break;
    }
    case SA::WarningMessage:
    {
        showWarningMessageInfo(info,interval);
        break;
    }
    default:
        showNormalMessageInfo(info,interval);
    }
}




QIcon MainWindow::getIconByWndType(SA::SubWndType type)
{
    switch(type)
    {
    case SA::SubWindowUserDefine:
    case SA::SubWindowFigure:
        return QIcon();
    default:
        return QIcon();
    }
    return QIcon();
}

///
/// \brief 提取图表中曲线的数据到sa中，以便用于其他的分析
/// \see pickCurData
///
void MainWindow::onActionPickCurveToDataTriggered()
{
    SAChart2D* chart = getCurSubWindowChart();
    if(!chart)
    {
        showWarningMessageInfo(tr("you should select a figure window"));
        return;
    }
    QList<QwtPlotCurve*> curs = CurveSelectDialog::getSelCurve(chart,this);
    if(curs.size()==0)
        return;
    PickCurveDataModeSetDialog pcDlg(this);
    if(QDialog::Accepted !=  pcDlg.exec())
        return;
    SA::DataSelectRange rang = pcDlg.getViewRange();
    SA::PickDataMode pickMode = pcDlg.getPickDataMode();
    for(auto i=curs.begin();i!=curs.end();++i)
    {
        QString name = QInputDialog::getText(this,QStringLiteral("数据命名")
                                             ,QStringLiteral("请为导出的数据命名："),QLineEdit::Normal,(*i)->title().text());
        QVector<QPointF> xy;
        if(SA::InSelectionRange == rang)
        {
            chart->getXYDataInRange(xy,nullptr,*i);
            makeValueFromXYSeries(name,pickMode,xy);
        }
        else
        {
            SAChart::getXYDatas(xy,(*i));
            makeValueFromXYSeries(name,pickMode,xy);
        }
    }
}
///
/// \brief 在当前标签中显示数据内容
///
void MainWindow::onActionViewValueInCurrentTabTriggered()
{
    QList<SAAbstractDatas*> datas=getSeletedDatas();
    if(datas.size ()<=0)
        return;
    ui->tabWidget_valueViewer->setDataInCurrentTab(datas);
}
///
/// \brief 在新标签中显示数据内容
///
void MainWindow::onActionViewValueInNewTabTriggered()
{
    QList<SAAbstractDatas*> datas=getSeletedDatas();
    if(datas.size ()<=0)
        return;
    ui->tabWidget_valueViewer->setDataInNewTab(datas);
}
///
/// \brief action 变量重命名
///
void MainWindow::onActionRenameValueTriggered()
{ 
    SAAbstractDatas* data = getSeletedData();
    if(data)
    {
        bool ok=false;
        QString name = QInputDialog::getText(this,QStringLiteral("输入新变量名")
                                             ,QStringLiteral("新变量名"),QLineEdit::Normal
                                             ,data->getName(),&ok);
        if(!ok)
            return;
        if(name == data->getName())
            return;
        if(name.isNull() || name.isEmpty())
        {
            QMessageBox::warning(this,QStringLiteral("警告"),QStringLiteral("请输入有效的变量名"));
            return;
        }
        saValueManager->renameData(data,name);
    }
}



///
/// \brief 获取选择的数据条目
///
/// 若在没有选中时需要用户显示选择，使用getSelectSingleData函数
/// \return 若没有在变量管理获取焦点，返回nullptr
/// \see getSelectSingleData
///
SAAbstractDatas* MainWindow::getSeletedData() const
{
    return ui->treeView_valueManager->getSeletedData();
}
///
/// \brief 获取选中的数据条目，如果没有选中，将弹出数据选择窗口让用户进行选择
/// 如果当前焦点是数据窗口会弹出选择数据，如果焦点是绘图窗口，默认选择图线
/// \param isAutoSelect 若为true时，会先查找变量管理器是否选中了数据，若选中了，直接返回选中的数据，
/// 默认为false
/// \return 若用户取消，返回nullptr
/// \see getSeletedData
///
SAAbstractDatas *MainWindow::getSelectSingleData(bool isAutoSelect)
{
    SAAbstractDatas *data = nullptr;
    data = getSeletedData();
    if(isAutoSelect)
    {
        if(data)
        {
            return data;
        }
    }
    QList<SAAbstractDatas*> tmp;
    SAValueSelectDialog dlg(this);
    dlg.setSelectMode(SAValueSelectDialog::SingleSelection);
    dlg.setDefaultSelectDatas({data});
    if(QDialog::Accepted == dlg.exec())
    {
        tmp = dlg.getSelectDatas();
    }

//    QList<SAAbstractDatas*> tmp
//            = SAValueSelectDialog::getSelectDatas(this,SAValueSelectDialog::SingleSelection);
    if(0==tmp.size())
    {
        return nullptr;
    }
    return tmp[0];
}
///
/// \brief 焦点变换触发的槽
/// \param old
/// \param now
///
void MainWindow::onFocusChanged(QWidget *old, QWidget *now)
{
    if(old && now)
    {
        if(old->metaObject() && now->metaObject())
        {
            qDebug() << "old widget:" << old->metaObject()->className()
                 << " new widget:" << now->metaObject()->className();
        }
        if(QwtPlotCanvas* c = qobject_cast<QwtPlotCanvas*>(now))
        {
            Q_UNUSED(c);
            m_lastForceType = SAUIInterface::FigureWindowFocus;
        }
        else if(SAValueManagerTreeView* v = qobject_cast<SAValueManagerTreeView*>(now))
        {
            Q_UNUSED(v);
            m_lastForceType = SAUIInterface::ValueManagerFocus;
        }
    }
}


///
/// \brief 获取选择的数据条目，此方法会自动调整选中的内容，如果选择的是说明，也会自动更改为对应的内容
/// \param index
/// \return
///
QList<SAAbstractDatas*> MainWindow::getSeletedDatas() const
{
    return ui->treeView_valueManager->getSeletedDatas();
}


SAPlotLayerModel* MainWindow::getPlotLayerModel() const
{
	return static_cast<SAPlotLayerModel*>(ui->tableView_layer->model());
}
///
/// \brief 把一个XYSeries转换为value
/// \param name 新变量名称
/// \param pickMode 抽取方式
/// \param xy 数据值
///
void MainWindow::makeValueFromXYSeries(const QString& name, SA::PickDataMode pickMode, const QVector<QPointF>& xy)
{
    std::shared_ptr<SAAbstractDatas> p = nullptr;
    if(SA::XOnly == pickMode)
    {
        QVector<double> x;
        SAChart::getXYDatas(xy,&x,nullptr);
        p = SAValueManager::makeData<SAVectorDouble>(name,x);//new SAVectorDouble(name,x);
    }
    else if(SA::YOnly == pickMode)
    {
        QVector<double> y;
        SAChart::getXYDatas(xy,nullptr,&y);
        p = SAValueManager::makeData<SAVectorDouble>(name,y);//new SAVectorDouble(name,y);
    }
    else if(SA::XYPoint == pickMode)
    {
        p = SAValueManager::makeData<SAVectorPointF>(name,xy);//new SAVectorPointF(name,xy);
    }
    saValueManager->addData(p);
}




#include <AboutDialog.h>
void MainWindow::onActionAboutTriggered()
{
    AboutDialog aboutDlg;
    aboutDlg.exec ();
}


///
/// \brief 显示延时状态栏信息
/// 延时信息显示在状态栏里，不同的信息类型显示不同的图标
/// \param info 信息内容
/// \param type 信息类型
/// \param interval 延时时间，默认4s
///
void MainWindow::showElapesdMessageInfo(const QString& info, SA::MeaasgeType type, int interval)
{
    ui_status_info->postInfo (info,type,interval);
}


///
/// \brief Rescind （回退）
///
void MainWindow::onActionUndoTriggered()
{
    SAFigureWindow* f = getCurrentFigureWindow();
    if(f)
    {
        if(f->isActiveWindow())
        {
            f->undo();
            return;
        }
    }
    if(ui->treeView_valueManager->isActiveWindow())
    {
        saValueManager->undo();
        return;
    }
}
///
/// \brief Redo （重做）
///
void MainWindow::onActionRedoTriggered()
{
    SAFigureWindow* f = getCurrentFigureWindow();
    if(f)
    {
        if(f->isActiveWindow())
        {
            f->redo();
            return;
        }
    }
    if(ui->treeView_valueManager->isActiveWindow())
    {
        saValueManager->redo();
        return;
    }
}

///
/// \brief 图层视图的点击
/// \param index
///
void MainWindow::onTableViewLayerPressed(const QModelIndex &index)
{
    if(!index.isValid ())
        return;

    QColor rgb = index.data (Qt::BackgroundColorRole).value<QColor>();
#if _CFG_LAYOUT_SELECT_CHANG_QSS
    ui->tableView_layer->setStyleSheet (getPlotLayerNewItemSelectedQSS(rgb));
#endif
    SAPlotLayerModel* model=getPlotLayerModel();
    QwtPlotItem* item = model->getPlotItemFromIndex (index);
    if (1==index.column())
    {
        model->setData (index,!item->isVisible ());
    }
    else if(index.column() == 2)
    {//颜色
        QColorDialog clrDlg;
        clrDlg.setCurrentColor(rgb);
        if(clrDlg.exec() == QDialog::Accepted)
        {
            QColor newClr = clrDlg.selectedColor();
#if _CFG_LAYOUT_SELECT_CHANG_QSS
            ui->tableView_layer->setStyleSheet (getPlotLayerNewItemSelectedQSS(newClr));
#endif
            model->setData (index,newClr,Qt::BackgroundColorRole);
        }
    }
    //
    SAChart2D* chart = getCurSubWindowChart();
    QItemSelectionModel* selMode = ui->tableView_layer->selectionModel();
    QSet<QwtPlotItem*> itemSets;
    if(selMode)
    {
        QModelIndexList selIndex = selMode->selectedRows();
        for(int i=0;i<selIndex.size();++i)
        {
            itemSets.insert( model->getPlotItemFromIndex (selIndex[i]));
        }
        chart->setCurrentSelectItems(itemSets.toList());
    }
}
#if _CFG_LAYOUT_SELECT_CHANG_QSS
///
/// \brief 根据已有的背景颜色来设置选中状态的背景颜色
/// \param rgb 已有的背景颜色
/// \return 设置的qss字符串
///
QString MainWindow::getPlotLayerNewItemSelectedQSS(const QColor& rgb)
{
#if 1
    QString qss = QString("QTableView::item:selected{selection-background-color:rgba(%1,%2,%3,100);}"
                          "QTableView::item:selected{selection-color:rgb(0,0,0);}")
                  .arg (rgb.red ()).arg (rgb.green ()).arg (rgb.blue ());
#else
    QString qss = QString("QTableView::item:selected{selection-background-color:rgba(255,255,255,255);}"
                          "QTableView::item:selected{border;3px solid %1;}"
                          "QTableView::item:selected{selection-color:rgb(0,0,0);}")
                  .arg (rgb.name());
#endif
    return qss;
}
#endif
bool MainWindow::setProjectInfomation()
{
    SAProjectInfomationSetDialog dlg(this);
    dlg.setProjectName(saProjectManager->getProjectName());
    dlg.setProjectDescribe(saProjectManager->getProjectDescribe());
    if(QDialog::Accepted != dlg.exec())
    {
        return false;
    }
    saProjectManager->setProjectName(dlg.getProjectName());
    saProjectManager->setProjectDescribe(dlg.getProjectDescribe());
    return true;
}
///
/// \brief 变量管理器的移除控制触发的槽
/// \param dataBeDeletedPtr 移除控制的数据
///
void MainWindow::onDataRemoved(const QList<SAAbstractDatas *> &dataBeDeletedPtr)
{
    ui->tabWidget_valueViewer->removeDatas(dataBeDeletedPtr);
}

void MainWindow::updateChartGridActionState(SAChart2D *chart)
{
    if(chart)
    {
        ui->actionShowGrid->setChecked(chart->isEnableGrid());
        ui->actionShowHGrid->setChecked(chart->isEnableGridY());
        ui->actionShowVGrid->setChecked(chart->isEnableGridX());
        ui->actionShowCrowdedHGrid->setChecked(chart->isEnableGridYMin());
        ui->actionShowCrowdedVGrid->setChecked(chart->isEnableGridXMin());
    }
    else
    {
        ui->actionShowGrid->setChecked(false);
        ui->actionShowHGrid->setChecked(false);
        ui->actionShowVGrid->setChecked(false);
        ui->actionShowCrowdedHGrid->setChecked(false);
        ui->actionShowCrowdedVGrid->setChecked(false);
    }
}
///
/// \brief 刷新选择工具
/// \param chart
///
void MainWindow::updateSelectActionState(SAChart2D *chart)
{
    if(chart)
    {
        SAAbstractRegionSelectEditor* editor = chart->getRegionSelectEditor();
        if(editor)
        {
            if(editor->isEnabled())
            {
                switch(editor->rtti())
                {
                case SAAbstractPlotEditor::RTTIRectRegionSelectEditor:
                    ui->ribbonButtonStartSelection->setDefaultAction(ui->actionStartRectSelect);
                    break;
                case SAAbstractPlotEditor::RTTIEllipseRegionSelectEditor:
                    ui->ribbonButtonStartSelection->setDefaultAction(ui->actionStartEllipseSelect);
                    break;
                case SAAbstractPlotEditor::RTTIPolygonRegionSelectEditor:
                    ui->ribbonButtonStartSelection->setDefaultAction(ui->actionStartPolygonSelect);
                    break;
                default:
                    ui->ribbonButtonStartSelection->setDefaultAction(ui->actionStartRectSelect);
                    break;
                }
                ui->ribbonButtonStartSelection->setChecked(true);
            }
            else
            {
                if(ui->ribbonButtonStartSelection->defaultAction()!=ui->actionStartRectSelect)
                {
                    ui->ribbonButtonStartSelection->setDefaultAction(ui->actionStartRectSelect);
                }
                ui->ribbonButtonStartSelection->setChecked(false);
            }
            //更新选择模式
            SAAbstractRegionSelectEditor::SelectionMode selMode = editor->getSelectionMode();
            switch(selMode)
            {
            case SAAbstractRegionSelectEditor::SingleSelection:
                ui->actionSingleSelection->setChecked(true);break;
            case SAAbstractRegionSelectEditor::AdditionalSelection:
                ui->actionAdditionalSelection->setChecked(true);break;
            case SAAbstractRegionSelectEditor::SubtractionSelection:
                ui->actionSubtractionSelection->setChecked(true);break;
            case SAAbstractRegionSelectEditor::IntersectionSelection:
                ui->actionIntersectionSelection->setChecked(true);break;
            default:
                ui->actionSingleSelection->setChecked(true);break;
            }

        }
        else
        {
            ui->ribbonButtonStartSelection->setChecked(false);
            ui->actionSingleSelection->setChecked(true);
        }
    }
    else
    {
        if(ui->ribbonButtonStartSelection->defaultAction()!=ui->actionStartRectSelect)
        {
            ui->ribbonButtonStartSelection->setDefaultAction(ui->actionStartRectSelect);
        }
        ui->ribbonButtonStartSelection->setChecked(false);
        ui->actionSingleSelection->setChecked(true);
    }
}

void MainWindow::updateChartEditorActionState(SAChart2D *chart)
{
    if(chart)
    {
        SAAbstractPlotEditor* editor = chart->getEditor();
        if(editor && editor->isEnabled())
        {
            switch(editor->rtti())
            {
            case SASelectRegionDataEditor::RTTISelectDataRegionEditor:
                ui->actionSelectionRegionDataMove->setChecked(true);break;
            case SASelectRegionEditor::RTTISelectRegionEditor:
                ui->actionSelectionRegionMove->setChecked(true);break;
            }

            return;
        }
    }
    ui->actionSelectionRegionMove->setChecked(false);
    ui->actionSelectionRegionDataMove->setChecked(false);

}

///
/// \brief 把窗口保存到文件夹
/// \param folder
///
void MainWindow::__saveSubWindowToFolder(const QString &folderPath)
{
    QList<QMdiSubWindow*> subWndList = this->getSubWindowList();
    //先把名字不对应的删除
    remove_figure_file_not_in_sub_window_list(folderPath,subWndList);
    //保存窗口
    const int count = subWndList.size();
    QString errString;
    for(int i=0;i<count;++i)
    {
        SAMdiSubWindow* subWnd = qobject_cast<SAMdiSubWindow*>(subWndList[i]);
        if(subWnd)
        {
            if(!save_sub_window(subWnd,folderPath,&errString))
            {
                showMessageInfo(errString,SA::WarningMessage);
            }
        }
    }
}

void MainWindow::__loadSubWindowFromFolder(const QString &folderPath)
{
    QDir dir(folderPath);
    if(!dir.exists())
    {
        showMessageInfo(tr("project may have not subwindow path :\"%1\"").arg(folderPath),SA::WarningMessage);
        return;
    }
    QString suffix = get_sub_window_type_suffix(SA::SubWindowFigure);
    QStringList dataFileList = dir.entryList({"*."+suffix},QDir::Files|QDir::NoSymLinks);
    const int size = dataFileList.size();
    if(0 == size)
    {
        return;
    }
    QString errString;
    QMdiSubWindow* sub = nullptr;
    for(int i=0;i<size;++i)
    {
        if(QMdiSubWindow* s = load_sub_window(uiInterface(),dir.absoluteFilePath(dataFileList[i]),&errString))
        {
            sub = s;
        }
        else
        {
            showMessageInfo(errString,SA::WarningMessage);
        }
    }
    if(sub)
    {
        setActiveSubWindow(sub);
    }
}






QwtPlotItemTreeModel *MainWindow::getDataViewerPlotItemTreeModel() const
{
    return static_cast<QwtPlotItemTreeModel*>(ui->treeView_curPlotItem->model());
}

QwtPlotItemDataModel *MainWindow::getDataViewerPlotItemDataModel() const
{
    return static_cast<QwtPlotItemDataModel*>(ui->tableView_curSelItemDatas->model());
}

///
/// \brief 曲线条目树形窗口
/// \param index
///
void MainWindow::onTreeViewCurPlotItemClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    QItemSelectionModel* sel = ui->treeView_curPlotItem->selectionModel();
    QModelIndexList indexList = sel->selectedRows();
    QwtPlotItemTreeModel* treeModel = getDataViewerPlotItemTreeModel();
    QwtPlotItemDataModel* tableModel = getDataViewerPlotItemDataModel();
    if(!treeModel)
        return;
    QList<QwtPlotItem*> items;
    for(int i = 0;i<indexList.size ();++i)
    {
        if(!indexList[i].parent().isValid())
        {//说明点击的是父窗口，就是qwtplot,这时显示所有
            items.clear();
            int childIndex = 0;
            while (indexList[i].child(childIndex,0).isValid()) {
                items.append(treeModel->getQwtPlotItemFromIndex (
                                 indexList[i].child(childIndex,0)));
                ++childIndex;
            }
            break;
        }
        if(indexList[i].column () != 0)
        {
            indexList[i] = indexList[i].parent().child(indexList[i].row(),0);
        }
        items.append (treeModel->getQwtPlotItemFromIndex (indexList[i]));
    }
    tableModel->setQwtPlotItems (items);
}




///
/// \brief 删除变量
///
void MainWindow::onActionDeleteValueTriggered()
{
    QList<SAAbstractDatas*> datas = getSeletedDatas();
    QString info=tr("Are you sure remove:\n");
    for(int i=0;i<datas.size () && i < 5;++i)
    {
        info += QStringLiteral("%1;").arg(datas[i]->text ());
    }
    info += tr("\n datas?");
    QMessageBox::StandardButton btn = QMessageBox::question (this,tr("Quesstion"),info);
    if(QMessageBox::Yes != btn)
    {
        return;
    }
    saValueManager->removeDatas(datas);
}






///
/// \brief 保存子窗口到文件
/// \param w 子窗口
/// \param folderPath 保存的文件夹路径
/// \param errString 错误提示
///
bool save_sub_window(SAMdiSubWindow *w,const QString& folderPath,QString* errString)
{
    QString title = w->windowTitle();
    QString suffix = get_sub_window_type_suffix(w->getType());
    QString filePath = QDir::cleanPath(folderPath) + QDir::separator() + title + "." + suffix;
    QFile file(filePath);
    QWidget* innerWidget = w->widget();
    switch(w->getType())
    {
    case SA::SubWindowFigure:
    {
        if(!file.open(QIODevice::WriteOnly))
        {
           if(errString)
           {
               *errString = file.errorString();
           }
           return false;
        }
        SAFigureWindow* fig = qobject_cast<SAFigureWindow*>(innerWidget);
        if(fig)
        {
            SAMdiSubWindowSerializeHead header;
            header.setValid();
            header.param.type = (int)w->getType();
            QDataStream out(&file);
            out << header;
            out << w->windowTitle();
            out << fig;
        }
        break;
    }
    default:
        break;
    }
    return true;

}
///
/// \brief 从文件加载子窗口
/// \param w 主对话框
/// \param filePath 文件路径
/// \param errString 错误信息
/// \return
///
QMdiSubWindow* load_sub_window(SAUIInterface *ui, const QString &filePath, QString *errString)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        if(errString)
        {
            *errString = file.errorString();
        }
        return nullptr;
    }
    try
    {
        SAMdiSubWindowSerializeHead header;
        QDataStream in(&file);
        in >> header;
        if(!header.isValid())
        {
            if(errString)
            {
                *errString = QObject::tr("invalid file");
            }
            return nullptr;
        }
        QString windowTitle;
        in >> windowTitle;
        QMdiSubWindow* sub = nullptr;
        switch(header.param.type)
        {
        case SA::SubWindowFigure:
        {
            sub = ui->createFigureWindow(windowTitle);
            SAFigureWindow* fig = qobject_cast<SAFigureWindow*>(sub->widget());
            if(fig)
            {
                in >> fig;
            }
            sub->show();
        }
        default:
            break;
        }
        return sub;
    }
    catch(const sa::SABadSerializeExpection& exp)
    {
        if(errString)
        {
            *errString = TR("unable load window:%1").arg(filePath);
        }
        saWarning("load figure window error:%s",exp.what());
    }
    catch(...)
    {
        if(errString)
        {
            *errString = TR("unable load window:%1").arg(filePath);
        }
        saWarning("load figure window error");
    }
    return nullptr;
}

QString get_sub_window_type_suffix(SA::SubWndType type)
{
    switch(type)
    {
    case SA::SubWindowFigure:
        return "saFig";
    default:
        break;
    }
    return "saSubWnd";
}

///
/// \brief 把没能和SubWindowList对应的文件删除
/// \param folderPath
/// \param subWindows
///
void remove_figure_file_not_in_sub_window_list(const QString &folderPath, const QList<QMdiSubWindow *> &subWindows)
{
    QDir dir(folderPath);
    if(!dir.exists())
    {
        return;
    }

    QStringList figureSubWindowFileNames;
    const int size = subWindows.size();
    QString suffix = get_sub_window_type_suffix(SA::SubWindowFigure);
    for(int i=0;i<size;++i)
    {
        SAMdiSubWindow* w = qobject_cast<SAMdiSubWindow*>(subWindows[i]);
        if(nullptr == w)
            continue;
        if(SA::SubWindowFigure == w->getType())
        {
            figureSubWindowFileNames.append(w->windowTitle()+ "." + suffix);
        }
    }
    QString cleanFolderPath = QDir::cleanPath(folderPath);
    QStringList dataFileList = dir.entryList({"*."+suffix},QDir::Files|QDir::NoSymLinks);
    const int fileSize = dataFileList.size();
    for(int i=0;i<fileSize;++i)
    {
        if(!figureSubWindowFileNames.contains(dataFileList[i]))
        {
            QFile::remove(cleanFolderPath + QDir::separator() + dataFileList[i]);
        }
    }
}
