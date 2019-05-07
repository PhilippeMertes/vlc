#include "dialogs/pvd_stats.hpp"

extern "C" {
    #include <libpvd.h>
    #include <vlc_tls.h>
}

#include <iostream>

#include <QTabWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QVector>
#include <QLabel>
#include <QLineEdit>

QVector<PvdStatsPanel*> PvdStatsDialog::panels = QVector<PvdStatsPanel*>();

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
    char *pvdname;
    if(pvd_get_pvd_list_sync(conn, pvd_list)) {
        msg_Warn(p_intf, "Error on getting PvD list from daemon.\n"
                        "Make sure pvdd is running on port 10101.");
        QMessageBox::warning(this, "No connection to pvdd",
                             "Error on getting PvD list from daemon.\n"
                             "Make sure pvdd is running on port 10101.");
    } else {
        for(int i = 0; i < pvd_list->npvd; ++i) {
            pvdname = strdup(pvd_list->pvdnames[i]);
            panels.push_back(new PvdStatsPanel(pvdTabW, p_intf, pvdname));
            pvdTabW->addTab(panels[i], qtr(pvdname));
            free(pvdname);
        }
    }

    free(pvd_list);

    /* TODO: remove after testing
    panels.push_back(new PvdStatsPanel(pvdTabW, p_intf, "video.mpvd.io."));
    pvdTabW->addTab(panels[0], qtr("video.mpvd.io."));
    if (panels[0]->isVisible()) {
        std::cout << "panels[0] is visible" << std::endl;
    }
    panels.push_back(new PvdStatsPanel(pvdTabW, p_intf, "test1.example.com."));
    pvdTabW->addTab(panels[1], qtr("test1.example.com."));
    if (panels[1]->isVisible()) {
        std::cout << "panels[1] is visible" << std::endl;
    }
     */

    // get and print the PvD the process is currently bound to
    QLabel *currPvdLabel = new QLabel(qtr("Current Pvd:"));
    pvdname = vlc_tls_GetCurrentPvd();
    currPvdLine = new QLineEdit;
    currPvdLine->setReadOnly(true);
    currPvdLine->setText(pvdname);
    free(pvdname);

    /* Bind to PvD button creation */
    QPushButton *bindButton = new QPushButton(qtr("&Bind"));
    bindButton->setDefault(true);
    BUTTONACT(bindButton, bind_to_pvd());

    /* Close button creation */
    QPushButton *closeButton = new QPushButton(qtr("&Close"));
    closeButton->setDefault(true);
    BUTTONACT(closeButton, close());

    /* Configure window layout */
    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(pvdTabW, 0, 0, 1, 8);
    layout->addWidget(currPvdLabel, 1, 0, 1, 1);
    layout->addWidget(currPvdLine, 1, 1, 1, 4);
    layout->addWidget(bindButton, 1, 6);
    layout->addWidget(closeButton, 1, 7);

    restoreWidgetPosition("PvdStats", QSize(600, 480));

    /* start thread handling connection with pvd-stats */
    if(vlc_clone(&pvd_stats_th, update_stats, NULL, VLC_THREAD_PRIORITY_LOW)) {
        msg_Err(p_intf, "Unable to create thread polling PvD statistics");
        QMessageBox::critical(this, "Error on thread creation", "Unable to create thread polling PvD statistics");
    }
}

PvdStatsDialog::~PvdStatsDialog()
{
    saveWidgetPosition("PvdStats");
}

void *PvdStatsDialog::update_stats(void *args)
{
    int idx;

    while (1) {
        if ((idx = visible_panel()) >= 0) {
            panels[idx]->update();
            panels[idx]->compare_stats_expected();
        }
        sleep(3);
    }
}

int PvdStatsDialog::visible_panel() {
    for (int i = 0; i < panels.size(); ++i) {
        if (panels[i]->isVisible())
            return i;
    }
    return -1;
}

void PvdStatsDialog::bind_to_pvd() {
    int idx;
    if ((idx = visible_panel()) >= 0) {
        std::string pvdname = panels[idx]->get_pvdname();
        switch (vlc_tls_BindToPvd(pvdname.c_str())) {
            case 0:
                QMessageBox::information(this, "Successful PvD binding", "Process is successfully bound to the PvD");
                break;

            case 1:
                QMessageBox::warning(this, "Failed binding to PvD",
                                     "Process failed binding to the PvD, thus remains unbound to any PvD.");
                break;

            case 2:
                QMessageBox::critical(this, "Failed binding to PvD",
                                      "Process failed binding to PvD and as well failed unbinding.");
        }
    }
}