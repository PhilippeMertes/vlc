#ifndef VLC_QT_INFO_PANELS_H_
#define VLC_QT_INFO_PANELS_H_

#include "util/qvlcframe.hpp"

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

//Inspired by InputStatsPanel class in info_panels.hpp
class PvdStatsPanel : public QWidget
{
    Q_OBJECT

public:
    PvdStatsPanel(QWidget *, intf_thread_t *);


private:
    QTreeWidget *statsTree;
    QTreeWidgetItem *throughput;
    QTreeWidgetItem *latency;

    vlc_thread_t pvd_stats_th;
    static intf_thread_t *p_intf;

    static void *update_stats(void *args);
};

#endif //VLC_QT_INFO_PANELS_H_