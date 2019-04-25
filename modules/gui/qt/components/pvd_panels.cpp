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


PvdStatsPanel::PvdStatsPanel(QWidget *parent, intf_thread_t *_p_intf, char *_pvdname) : QWidget(parent)
{
    if (p_intf == NULL)
        p_intf = _p_intf;

    pvdname = _pvdname;
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

#define CREATE_AND_ADD_TO_CAT( itemName, itemText, itemValue, catName, unit ) { \
    CREATE_TREE_ITEM( itemName, itemText, itemValue, unit );                   \
    catName->addChild( itemName ); }

    CREATE_CATEGORY(tput, qtr("Throughput"));
    CREATE_CATEGORY(rtt, qtr("Round-trip time"));

    CREATE_AND_ADD_TO_CAT(tput_gen, qtr("General"), "", tput, qtr(""));
    CREATE_AND_ADD_TO_CAT(tput_avg, qtr("Avg"), "0", tput_gen, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_min, qtr("Min"), "0", tput_gen, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_max, qtr("Max"), "0", tput_gen, qtr("Mb/s"));

    CREATE_AND_ADD_TO_CAT(rtt_avg, qtr("Avg"), "0", rtt, qtr("s"));
    CREATE_AND_ADD_TO_CAT(rtt_min, qtr("Min"), "0", rtt, qtr("s"));
    CREATE_AND_ADD_TO_CAT(rtt_max, qtr("Max"), "0", rtt, qtr("s"));

#undef CREATE_AND_ADD_TO_CAT
#undef CREATE_CATEGORY
#undef CREATE_TREE_ITEM

    tput->setExpanded(true);
    rtt->setExpanded(true);

    /* Configure layout */
    statsTree->resizeColumnToContents(0);
    statsTree->setColumnWidth(1, 200);
    layout->addWidget(statsTree, 4, 0);
}



void PvdStatsPanel::update_parse_json(const QJsonObject& json) {
#define UPDATE_FLOAT( widget, format, calc... ) \
    { QString str; widget->setText( 1 , str.sprintf( format, ## calc ) );  }

    //const char* stats_keys[3] = {"general", "upload", "download"};
    const char* val_keys[3] = {"min", "max", "avg"};
    QTreeWidgetItem* tput_widgets[3] = {tput_min, tput_max, tput_avg};
    QTreeWidgetItem* rtt_widgets[3] = {rtt_min, rtt_max, rtt_avg};
    QJsonValue val;
    QJsonObject obj;

    // update throughput information
    QString key = QString("tput");
    if (json.contains(key)) {
        obj = json.value(key).toObject();
        key = QString("general");
        obj = obj.value(key).toObject();

        for (int i = 0; i < 3; ++i) {
            key = QString(val_keys[i]);
            if (obj.contains(key)) {
                val = obj.value(key);
                std::cout << key.toStdString() << ": " << val.toDouble() << std::endl;
                UPDATE_FLOAT(tput_widgets[i], "%.6f", val.toDouble());
            }
        }

    }

    // update RTT information
    key = QString("rtt");
    if (json.contains(key)) {
        obj = json.value(key).toObject();
        obj = obj.value(QString("general")).toObject();
        for (int i = 0; i < 3; ++i) {
            key = QString(val_keys[i]);
            val = obj.value(key);
            UPDATE_FLOAT(rtt_widgets[i], "%.6f", val.toDouble());
        }
    }

#undef UPDATE_FLOAT
}


void PvdStatsPanel::update() {
    std::cout << "updating panel " << pvdname << std::endl;
    using ::close;
    int sock;
    struct sockaddr_un addr;
    std::string msg = "all " + pvdname;
    std::cout << msg << std::endl;
    char resp[2048];
    QString json_str;
    QJsonObject json;
    QJsonDocument json_doc;

    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, SOCKET_FILE);
    // create socket
    if ((sock = socket(AF_LOCAL, SOCK_STREAM, 0)) <= 0) {
        msg_Err(p_intf, "Unable to create socket to communicate with pvd-stats\n");
        return;
    }

    // connect to the pvd-stats socket
    if (::connect(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        msg_Warn(p_intf, "Unable to connect to the pvd-stats socket:\n%s\n", strerror(errno));
        QMessageBox::warning(this, "pvd-stats connection error",
                "Unable to connect to the pvd-stats socket.\nCheck if it is running.");
        close(sock);
        return;
    }

    // send message
    send(sock, msg.c_str(), msg.length(), 0);

    // receive answer
    while(recv(sock, resp, 2048, 0) > 0) {
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
            return;
        }
        update_parse_json(json);
    }
    json_str.clear();
    close(sock);
}

