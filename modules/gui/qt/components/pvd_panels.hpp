#ifndef VLC_QT_INFO_PANELS_H_
#define VLC_QT_INFO_PANELS_H_

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

//Inspired by InputStatsPanel class in info_panels.hpp
class PvdStatsPanel : public QWidget
{
    Q_OBJECT

public:
    PvdStatsPanel(QWidget *);

private:
    QTreeWidget *statsTree;
    QTreeWidgetItem *throughput;
    QTreeWidgetItem *latency;
};

#endif //VLC_QT_INFO_PANELS_H_