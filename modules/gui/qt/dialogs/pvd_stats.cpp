#include "dialogs/pvd_stats.hpp"

extern "C" {
    #include <libpvd.h>
    #include <vlc_tls.h>
    #include <vlc_network.h>
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
QLineEdit *PvdStatsDialog::currPvdLine = NULL;

/**
 * Dialog window presenting networking statistics by Provisioning Domains (PvDs).
 *
 * @param _p_intf: thread interface
 */
PvdStatsDialog::PvdStatsDialog(intf_thread_t *_p_intf) : QVLCFrame(_p_intf)
{
    char *pvdname;

    /* Window information */
    setWindowTitle(qtr("Provisioning Domains Statistics"));
    setWindowRole("vlc-pvd-stats");

    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint |
                    Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);

    /* get list of PvD names */
    t_pvd_connection *conn = pvd_connect(-1);
    t_pvd_list *pvd_list = (t_pvd_list*) malloc(sizeof(t_pvd_list));
    if (!pvd_list) {
        msg_Err(p_intf, "Unable to allocate memory to hold "
                        "the list of Provisioning Domains.");
    }
    else {
        /* TabWidgets and Tabs creation, tabs named after PvDs */
        pvdTabW = new QTabWidget;
        if(pvd_get_pvd_list_sync(conn, pvd_list)) {
            msg_Warn(p_intf, "Error on getting PvD list from daemon.\n"
                             "Make sure pvdd is running on port 10101.");
            QMessageBox::warning(this, "No connection to pvdd",
                                 "Error on getting PvD list from daemon.\n"
                                 "Make sure pvdd is running on port 10101.");
        } else {
            for(int i = 0; i < pvd_list->npvd; ++i) {
                // create tab panels
                pvdname = pvd_list->pvdnames[i];
                panels.push_back(new PvdStatsPanel(pvdTabW, p_intf, pvdname));
                pvdTabW->addTab(panels[i], qtr(pvdname));
                free(pvdname);
            }
        }
        free(pvd_list);
    }
    pvd_disconnect(conn);

    // get and print the PvD the process is currently bound to
    QLabel *currPvdLabel = new QLabel(qtr("Current Pvd:"));
    pvdname = vlc_GetCurrentPvd();
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
    qRegisterMetaType<QVector<int> >("int_vector"); // needed, or else we get error messages during thread creation
    if(vlc_clone(&pvd_stats_th, update_stats, NULL, VLC_THREAD_PRIORITY_LOW)) {
        msg_Err(p_intf, "Unable to create thread polling PvD statistics");
        QMessageBox::critical(this, "Error on thread creation", "Unable to create thread polling PvD statistics");
    }
}

PvdStatsDialog::~PvdStatsDialog()
{
    saveWidgetPosition("PvdStats");
}

/**
 * Updates the statistics for the currently visible panel every second.
 *
 * Should be performed in a new thread.
 *
 * @param args NULL
 * @return NULL
 */
void *PvdStatsDialog::update_stats(void *args)
{
    int idx;
    char *curr_pvd;

    while (1) {
        // find visible panel
        if ((idx = visible_panel()) >= 0) {
            // update stats
            panels[idx]->update();
            panels[idx]->compare_stats_expected();
        }
        // update current PvD
        curr_pvd = vlc_GetCurrentPvd();
        currPvdLine->setText(curr_pvd);
        free(curr_pvd);
        // sleep for one second
        sleep(1);
    }
}

/**
 * Determines the currently visible panel.
 * @return panel index or -1 if no panel is visible
 */
int PvdStatsDialog::visible_panel() {
    for (int i = 0; i < panels.size(); ++i) {
        if (panels[i]->isVisible())
            return i;
    }
    return -1;
}

/**
 * Binds VLC to the PvD of the currently visible panel.
 *
 * A message box will open indicating if the binding was successful.
 * If yes, the user may also have the possibility to set this PvD as its "preferred".
 */
void PvdStatsDialog::bind_to_pvd() {
    int idx;
    if ((idx = visible_panel()) >= 0) {
        std::string pvdname = panels[idx]->get_pvdname();
        switch (vlc_BindToPvd(pvdname.c_str())) {
            case 0: // Success
                QMessageBox::information(this, "Successful PvD binding", "Process is successfully bound to the PvD");
                currPvdLine->setText(pvdname.c_str());
                if (QMessageBox::question(this, "Set as preferred PvD",
                        "Do you want to set this PvD as your preferred?"))
                    vlc_tls_SetPreferredPvd(pvdname.c_str());
                break;

            case 1: // unsuccessful, but succeeded unbinding
                QMessageBox::warning(this, "Failed binding to PvD",
                                     "Process failed binding to the PvD.\nThus, it is now unbound to any PvD.");
                break;

            case 2: // binding and unbinding unsuccessful
                QMessageBox::critical(this, "Failed binding to PvD",
                                      "Process failed binding to PvD as well as unbinding!");
        }
    }
}