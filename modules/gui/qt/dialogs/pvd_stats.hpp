#ifndef QVLC_PVD_STATS_DIALOG_H_
#define QVLC_PVD_STATS_DIALOG_H_

#include "util/qvlcframe.hpp"
#include "components/pvd_panels.hpp"
#include "util/singleton.hpp"

class QTabWidget;

//Inspired by MediaInfoDialog class
class PvdStatsDialog : public QVLCFrame, public Singleton<PvdStatsDialog>
{
    Q_OBJECT

public:
    PvdStatsDialog(intf_thread_t *);

private:
    virtual ~PvdStatsDialog();

    QTabWidget *pvdTabW;
    vlc_thread_t pvd_stats_th;

    static QVector<PvdStatsPanel*> panels;

    static int visible_panel();
    static void *update_stats(void *args);

private slots:
    friend class Singleton<PvdStatsDialog>;
    void bind_to_pvd();

};

#endif //QVLC_PVD_STATS_DIALOG_H_
