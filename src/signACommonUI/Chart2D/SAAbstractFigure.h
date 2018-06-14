#ifndef SA_ABSTRACT_FIGURE_H
#define SA_ABSTRACT_FIGURE_H
#include "SACommonUIGlobal.h"
#include <QList>
class SAChart2D;
///
/// \brief sa的绘图窗口接口，所有绘图窗口都需要继承它
///
class SA_COMMON_UI_EXPORT SAAbstractFigure
{
public:
    SAAbstractFigure();
    virtual ~SAAbstractFigure();
    //添加一个2D chart
    virtual SAChart2D* create2DPlot() = 0;
    //添加一个2D sub chart
    virtual SAChart2D* create2DSubPlot(float xPresent, float yPresent, float wPresent, float hPresent) = 0;
    //获取所有的图表
    virtual QList<SAChart2D*> get2DPlots() const = 0;
    //获取当前的2d绘图指针
    virtual SAChart2D* current2DPlot() const = 0;

};

#endif // SAFIGURE_H
