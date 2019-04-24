#include "components/pvd_panels.hpp"

#include "qt.hpp"

extern "C" {
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <arpa/inet.h>
    #include <errno.h>
};

#include <iostream>
#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>


#define SOCKET_FILE "/tmp/pvd-stats.uds"


intf_thread_t* PvdStatsPanel::p_intf = NULL;


PvdStatsPanel::PvdStatsPanel(QWidget *parent, intf_thread_t *_p_intf) : QWidget(parent)
{
    if (p_intf == NULL) {
        p_intf = _p_intf;
        std::cout << "p_intf == NULL" << std::endl;
    }
    QVBoxLayout *layout = new QVBoxLayout(this);

    /* Label above the stats tree */
    QLabel *topLabel = new QLabel(qtr("Network statistics for this PvD"));
    topLabel->setWordWrap(true);
    layout->addWidget(topLabel, 0, 0);

    /* Create tree containing the statistics */
    statsTree = new QTreeWidget(this);
    statsTree->setColumnCount(2);
    statsTree->setHeaderHidden(true);

    /* Macros for functions used to build the tree */
#define CREATE_TREE_ITEM(itemName, itemText, itemValue, unit) {              \
    itemName =                                                               \
      new QTreeWidgetItem((QStringList() << itemText << itemValue << unit)); \
    itemName->setTextAlignment(1, Qt::AlignRight); }

#define CREATE_CATEGORY(catName, itemText) {                           \
    CREATE_TREE_ITEM(catName, itemText, "", "");                       \
    catName->setExpanded(true);                                        \
    statsTree->addTopLevelItem(catName); }

    CREATE_CATEGORY(throughput, qtr("Throughput"));
    CREATE_CATEGORY(latency, qtr("Latency"));

#undef CREATE_CATEGORY
#undef CREATE_TREE_ITEM

    /* Configure layout */
    statsTree->resizeColumnToContents(0);
    statsTree->setColumnWidth(1, 200);
    layout->addWidget(statsTree, 4, 0);

    /* start thread handling connection with pvd-stats */
    if(vlc_clone(&pvd_stats_th, update_stats, NULL, VLC_THREAD_PRIORITY_LOW)) {
        //msg_Err(p_intf, "Unable to create thread polling PvD statistics");
        QMessageBox::critical(this, "Error on thread creation", "Unable to create thread polling PvD statistics");
    }
}



void parse_json(QJsonObject& json) {
    QStringList keys = json.keys();
    for (int i = 0; i < keys.size(); ++i) {
        std::cout << keys.at(i).toLocal8Bit().constData() << std::endl;
    }
}


void *PvdStatsPanel::update_stats(void *args)
{
    using ::close;
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
        if ((sock = ::socket(AF_LOCAL, SOCK_STREAM, 0)) <= 0) {
            msg_Err(p_intf, "Unable to create socket to communicate with pvd-stats\n");
            break;
        }

        // connect to the pvd-stats socket
        if (::connect(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
            printf("Unable to connect to the pvd-stats socket\n%s\n", strerror(errno));
            msg_Err(p_intf, "Unable to connect to the pvd-stats socket:\n%s\n", strerror(errno));
            close(sock);
            break;
        }

        // send message
        ::send(sock, msg, strlen(msg), 0);

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
                break;
            }
            parse_json(json);
        }
        json_str.clear();
        close(sock);

        sleep(5);
    }

    return NULL;
}

