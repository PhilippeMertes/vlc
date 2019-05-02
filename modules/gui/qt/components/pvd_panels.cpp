#include "components/pvd_panels.hpp"

#include "qt.hpp"

extern "C" {
    #include <libpvd.h>
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
#define PVD_PORT 10101


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

    get_extra_info();

    /* Create tree containing the statistics */
    statsTree = new QTreeWidget(this);
    statsTree->setColumnCount(3);
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

#define CREATE_AND_ADD_TO_CAT(itemName, itemText, itemValue, catName, unit) { \
    CREATE_TREE_ITEM(itemName, itemText, itemValue, unit);                   \
    catName->addChild(itemName); }

    CREATE_CATEGORY(tput, qtr("Throughput"));
    CREATE_CATEGORY(rtt, qtr("Round-trip time"));

    if (exp_vals[0].empty()) {
        CREATE_AND_ADD_TO_CAT(tput_gen, qtr("In general"), "", tput, "");
    } else {
        CREATE_AND_ADD_TO_CAT(tput_gen, qtr("In general"), "expected:", tput, exp_vals[0].c_str());
    }
    CREATE_AND_ADD_TO_CAT(tput_avg, qtr("Avg"), "0", tput_gen, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_min, qtr("Min"), "0", tput_gen, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_max, qtr("Max"), "0", tput_gen, qtr("Mb/s"));

    if (exp_vals[1].empty()) {
        CREATE_AND_ADD_TO_CAT(tput_up, qtr("Upload"), "", tput, "");
    } else {
        CREATE_AND_ADD_TO_CAT(tput_up, qtr("Upload"), "expected:", tput, exp_vals[1].c_str());
    }
    CREATE_AND_ADD_TO_CAT(tput_up_avg, qtr("Avg"), "0", tput_up, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_up_min, qtr("Min"), "0", tput_up, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_up_max, qtr("Max"), "0", tput_up, qtr("Mb/s"));

    if (exp_vals[2].empty()) {
        CREATE_AND_ADD_TO_CAT(tput_dwn, qtr("Download"), "", tput, "");
    } else {
        CREATE_AND_ADD_TO_CAT(tput_dwn, qtr("Download"), "expected:", tput, exp_vals[2].c_str());
    }
    CREATE_AND_ADD_TO_CAT(tput_dwn_avg, qtr("Avg"), "0", tput_dwn, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_dwn_min, qtr("Min"), "0", tput_dwn, qtr("Mb/s"));
    CREATE_AND_ADD_TO_CAT(tput_dwn_max, qtr("Max"), "0", tput_dwn, qtr("Mb/s"));

    if (exp_vals[3].empty()) {
        CREATE_AND_ADD_TO_CAT(rtt_gen, qtr("In general"), "", rtt, "");
    } else {
        CREATE_AND_ADD_TO_CAT(rtt_gen, qtr("In general"), "expected:", rtt, exp_vals[3].c_str());
    }
    CREATE_AND_ADD_TO_CAT(rtt_avg, qtr("Avg"), "0", rtt_gen, qtr("\u00B5s"));
    CREATE_AND_ADD_TO_CAT(rtt_min, qtr("Min"), "0", rtt_gen, qtr("\u00B5s"));
    CREATE_AND_ADD_TO_CAT(rtt_max, qtr("Max"), "0", rtt_gen, qtr("\u00B5s"));

    if (exp_vals[4].empty()) {
        CREATE_AND_ADD_TO_CAT(rtt_up, qtr("Upload"), "", rtt, "");
    } else {
        CREATE_AND_ADD_TO_CAT(rtt_up, qtr("Upload"), "expected:", rtt, exp_vals[4].c_str());
    }
    CREATE_AND_ADD_TO_CAT(rtt_up_avg, qtr("Avg"), "0", rtt_up, qtr("\u00B5s"));
    CREATE_AND_ADD_TO_CAT(rtt_up_min, qtr("Min"), "0", rtt_up, qtr("\u00B5s"));
    CREATE_AND_ADD_TO_CAT(rtt_up_max, qtr("Max"), "0", rtt_up, qtr("\u00B5s"));

    if (exp_vals[5].empty()) {
        CREATE_AND_ADD_TO_CAT(rtt_dwn, qtr("Download"), "", rtt, "");
    } else {
        CREATE_AND_ADD_TO_CAT(rtt_dwn, qtr("Download"), "expected:", rtt, exp_vals[5].c_str());
    }
    CREATE_AND_ADD_TO_CAT(rtt_dwn_avg, qtr("Avg"), "0", rtt_dwn, qtr("\u00B5s"));
    CREATE_AND_ADD_TO_CAT(rtt_dwn_min, qtr("Min"), "0", rtt_dwn, qtr("\u00B5s"));
    CREATE_AND_ADD_TO_CAT(rtt_dwn_max, qtr("Max"), "0", rtt_dwn, qtr("\u00B5s"));

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

    const char* stats_keys[2] = {"tput", "rtt"};
    const char* stats_subkeys[3] = {"general", "upload", "download"};
    const char* val_keys[3] = {"min", "max", "avg"};
    QTreeWidgetItem* tput_widgets[3][3] = {
            {tput_min, tput_max, tput_avg},
            {tput_up_min, tput_up_max, tput_up_avg},
            {tput_dwn_min, tput_dwn_max, tput_dwn_avg}
    };
    QTreeWidgetItem* rtt_widgets[3][3] = {
            {rtt_min, rtt_max, rtt_avg},
            {rtt_up_min, rtt_up_max, rtt_up_avg},
            {rtt_dwn_min, rtt_dwn_max, rtt_dwn_avg}
    };
    QJsonValue val;
    QJsonObject stats_obj;
    QJsonObject obj;
    QString key;

    for (int i = 0; i < 2; ++i) {
        key = QString(stats_keys[i]);
        if (!json.contains(key))
            continue;
        stats_obj = json.value(key).toObject();
        // iterate through general, upload and download statistics
        for (int j = 0; j < 3; ++j) {
            key = QString(stats_subkeys[j]);
            if (!stats_obj.contains(key))
                continue;
            obj = stats_obj.value(key).toObject();
            // get min, max and avg
            for (int k = 0; k < 3; ++k) {
                key = QString(val_keys[k]);
                if (!obj.contains(key))
                    continue;
                val = obj.value(key);
                if (i == 0) { // update throughput
                    UPDATE_FLOAT(tput_widgets[j][k], "%.6f", val.toDouble());
                } else { // update RTT
                    UPDATE_FLOAT(rtt_widgets[j][k], "%.3f", val.toDouble() * 1000000); // print RTT in us
                }
            }
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
        msg_Dbg(p_intf, "pvd-stats answer:\n%s", json_str.toStdString().c_str());
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

void PvdStatsPanel::get_extra_info() {
    t_pvd_connection *conn = pvd_connect(PVD_PORT);
    char *extra_info;
    QJsonObject json;
    QJsonDocument json_doc;
    QString key;
    QJsonValue val;
    const char *extra_info_keys[2][3] = {
            {"tput", "tput_up", "tput_dwn"},
            {"rtt", "rtt_up", "rtt_dwn"}
    };

    pvd_get_attribute_sync(conn, const_cast<char*>(pvdname.c_str()), const_cast<char*>("extraInfo"), &extra_info);
    std::cout << "extra_info = \"" << extra_info << "\"" << std::endl;

    if (strcmp(extra_info, "null\n") == 0) { // no extra info known to pvdd
        msg_Warn(p_intf, "No additional information for the PvD \"%s\" known by pvdd.", pvdname.c_str());
        delete extra_info;
        pvd_disconnect(conn);
        return;
    }

    // convert string to QJsonObject
    json_doc = QJsonDocument::fromJson(QString(extra_info).toUtf8());
    if ((!json_doc.isNull() && json_doc.isObject()))
        json = json_doc.object();
    else {
        msg_Warn(p_intf, "pvdd didn't return a JSON object\nResponse:\n%s", extra_info);
        delete extra_info;
        pvd_disconnect(conn);
        return;
    }

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 3; ++j) {
            key = QString(extra_info_keys[i][j]);
            if (json.contains(key)) {
                val = json.value(key);
                if (val.type() == QJsonValue::String) {
                    exp_vals[i*3+j] = val.toString().toStdString();
                    continue;
                }
            }
        }
    }
    
    delete extra_info;
    pvd_disconnect(conn);
}

