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
    PvdStatsPanel(QWidget *, intf_thread_t *, char *);

    void update();


private:
    QTreeWidget *statsTree;
    QTreeWidgetItem *tput;
    QTreeWidgetItem *tput_gen;
    QTreeWidgetItem *tput_avg;
    QTreeWidgetItem *tput_min;
    QTreeWidgetItem *tput_max;
    QTreeWidgetItem *tput_dwn;
    QTreeWidgetItem *tput_dwn_avg;
    QTreeWidgetItem *tput_dwn_min;
    QTreeWidgetItem *tput_dwn_max;
    QTreeWidgetItem *tput_up;
    QTreeWidgetItem *tput_up_avg;
    QTreeWidgetItem *tput_up_min;
    QTreeWidgetItem *tput_up_max;


    QTreeWidgetItem *rtt;
    QTreeWidgetItem *rtt_gen;
    QTreeWidgetItem *rtt_avg;
    QTreeWidgetItem *rtt_min;
    QTreeWidgetItem *rtt_max;
    QTreeWidgetItem *rtt_dwn;
    QTreeWidgetItem *rtt_dwn_avg;
    QTreeWidgetItem *rtt_dwn_min;
    QTreeWidgetItem *rtt_dwn_max;
    QTreeWidgetItem *rtt_up;
    QTreeWidgetItem *rtt_up_avg;
    QTreeWidgetItem *rtt_up_min;
    QTreeWidgetItem *rtt_up_max;

    std::string pvdname;
    static intf_thread_t *p_intf;

    void update_parse_json(const QJsonObject& json);
};

#endif //VLC_QT_INFO_PANELS_H_