#include "dialogs/pvd_stats.hpp"

#include <QTabWidget>
#include <QGridLayout>
#include <QPushButton>

PvdStatsDialog::PvdStatsDialog(intf_thread_t *_p_intf) : QVLCFrame(_p_intf)
{
   /* Window information */
   setWindowTitle(qtr("Provisioning Domains Statistics"));
   setWindowRole("vlc-pvd-stats");

    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint |
                    Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);


    /* TabWidgets and Tabs creation */
    pvdTabW = new QTabWidget;
    char tab_string[256];
    for(int i = 0; i < 3; ++i) {
        sprintf(tab_string, "PvD %d", i);
        pvdTabW->addTab(new PvdStatsPanel(pvdTabW), qtr(tab_string));
    }

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