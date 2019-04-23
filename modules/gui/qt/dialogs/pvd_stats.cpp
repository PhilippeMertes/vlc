#include "dialogs/pvd_stats.hpp"

extern "C" {
    #include <libpvd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <arpa/inet.h>
    #include <errno.h>
}

#include <iostream>
#include <QTabWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>


#define SOCKET_FILE "/tmp/pvd-stats.uds"

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
        //char *pvdname;
        pvdnames = new char*[pvd_list->npvd];
        for(int i = 0; i < pvd_list->npvd; ++i) {
            pvdnames[i] = strdup(pvd_list->pvdnames[i]);
            pvdTabW->addTab(new PvdStatsPanel(pvdTabW), qtr(pvdnames[i]));
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

    /* start thread handling connection with pvd-stats */
    if(vlc_clone(&pvd_stats_th, update_stats, &_p_intf, VLC_THREAD_PRIORITY_INPUT)) {
        msg_Err(p_intf, "Unable to create thread polling PvD statistics");
        QMessageBox::critical(this, "Error on thread creation", "Unable to create thread polling PvD statistics");
    }
}

PvdStatsDialog::~PvdStatsDialog()
{
    saveWidgetPosition("PvdStats");
    delete pvdnames;
}


void parse_json(QJsonObject& json) {
    QStringList keys = json.keys();
    for (int i = 0; i < keys.size(); ++i) {
        std::cout << keys.at(i).toLocal8Bit().constData() << std::endl;
    }
}


void *update_stats(void *args)
{
    intf_thread_t *p_intf = (intf_thread_t*) args;
    int sock;
    struct sockaddr_un addr;
    char msg[] = "all";
    char resp[2048];
    QString json_str;
    QJsonObject json;
    QJsonDocument json_doc;
    int ret;

    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, SOCKET_FILE);

    // update the statistics every 5 seconds
    while(1) {
        // create socket
        if ((sock = socket(AF_LOCAL, SOCK_STREAM, 0)) <= 0) {
            msg_Err(p_intf, "Unable to create socket to communicate with pvd-stats\n");
            return NULL;
        }

        // connect to the pvd-stats socket
        if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
            printf("Unable to connect to the pvd-stats socket\n%s\n", strerror(errno));
            msg_Err(p_intf, "Unable to connect to the pvd-stats socket:\n%s\n", strerror(errno));
            close(sock);
            return NULL;
        }

        // send message
        send(sock, msg, strlen(msg), 0);

        // receive answer
        while((ret = recv(sock, resp, 2048, 0)) > 0) {
            json_str.append(resp);
        }

        if (!json_str.isEmpty()) {
            std::cout << "pvd-stats answer:\n" << json_str.toStdString() << std::endl;
            // convert string to QJsonObject
            json_doc = QJsonDocument::fromJson(json_str.toUtf8());
            if ((!json_doc.isNull() && json_doc.isObject()))
                json = json_doc.object();
            else {
                msg_Err(p_intf, "PvD-Stats answer was not a JSON object\nAnswer:\n%s",
                        json_str.toStdString().c_str());
                close(sock);
                return NULL;
            }
            parse_json(json);
        }
        json_str.clear();
        close(sock);

        sleep(5);
    }

    return NULL;
}