#include "components/pvd_panels.hpp"

#include "qt.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>

PvdStatsPanel::PvdStatsPanel(QWidget *parent) : QWidget(parent)
{
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
}

