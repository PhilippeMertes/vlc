#include "dialogs/pvd_stats.hpp"

extern "C" {
    #include <libpvd.h>
}

#include <QTabWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QMessageBox>

PvdStatsDialog::PvdStatsDialog(intf_thread_t *_p_intf) : QVLCFrame(_p_intf)
{
   /* Window information */
   setWindowTitle(qtr("Provisioning Domains Statistics"));
   setWindowRole("vlc-pvd-stats");

    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint |
                    Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);

    /* get list of PvD names */
    t_pvd_connection *conn = pvd_connect(10101);
    t_pvd_list *pvd_list = (t_pvd_list*) malloc(sizeof(t_pvd_list));

    /* TabWidgets and Tabs creation, tabs named after PvDs */
    pvdTabW = new QTabWidget;
    if(pvd_get_pvd_list_sync(conn, pvd_list)) {
        msg_Warn(p_intf, "Error on getting PvD list from daemon.\n"
                        "Make sure pvdd is running on port 10101.");
        QMessageBox::warning(this, "No connection to pvdd",
                             "Error on getting PvD list from daemon.\n"
                             "Make sure pvdd is running on port 10101.");
    } else {
        char *pvdname;
        for(int i = 0; i < pvd_list->npvd; ++i) {
            pvdname = strdup(pvd_list->pvdnames[i]);
            pvdTabW->addTab(new PvdStatsPanel(pvdTabW), qtr(pvdname));
        }
    }

    free(pvd_list);

    /* Close button creation */
    QPushButton *closeButton = new QPushButton(qtr("&Close"));
    closeButton->setDefault(true);

    /* Configure window layout */
    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(pvdTabW, 0, 0, 1, 8);
    layout->addWidget(closeButton, 1, 7);

    BUTTONACT(closeButton, close());

    restoreWidgetPosition("PvdStats", QSize(600, 480));
}

PvdStatsDialog::~PvdStatsDialog()
{
    saveWidgetPosition("PvdStats");
}