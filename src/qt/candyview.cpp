// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "candyview.h"

#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactiondescdialog.h"
#include "transactionfilterproxy.h"
#include "transactionrecord.h"
#include "candytablemodel.h"
#include "walletmodel.h"

#include "ui_interface.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QScrollBar>
#include <QSettings>
#include <QSignalMapper>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>
#include "customdoublevalidator.h"

/** Date format for persistence */
static const char* PERSISTENCE_DATE_FORMAT = "yyyy-MM-dd";
extern CAmount gFilterAmountMaxNum;

CandyView::CandyView(const PlatformStyle *platformStyle, QWidget *parent):
     QWidget(parent),model(0), transactionProxyModel(0),
    candyView(0), abandonAction(0), columnResizingFixer(0)
{
    QSettings settings;
    // Build filter row
    setContentsMargins(0,0,0,0);

    useExtraSpacing = platformStyle->getUseExtraSpacing();
    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0,0,0,0);
    if (platformStyle->getUseExtraSpacing()) {
        hlayout->setSpacing(0);
        hlayout->addSpacing(6);
    } else {
        hlayout->setSpacing(1);
        hlayout->addSpacing(5);
    }
    QString theme = GUIUtil::getThemeName();
    watchOnlyWidget = new QComboBox(this);
    watchOnlyWidget->setFixedWidth(24);
    watchOnlyWidget->addItem("", TransactionFilterProxy::WatchOnlyFilter_All);
    watchOnlyWidget->addItem(QIcon(":/icons/" + theme + "/eye_plus"), "", TransactionFilterProxy::WatchOnlyFilter_Yes);
    watchOnlyWidget->addItem(QIcon(":/icons/" + theme + "/eye_minus"), "", TransactionFilterProxy::WatchOnlyFilter_No);
    hlayout->addWidget(watchOnlyWidget);

    dateWidget = new QComboBox(this);
    if (platformStyle->getUseExtraSpacing()) {
        dateWidget->setFixedWidth(GUIUtil::DATE_COLUMN_WIDTH);
    } else {
        dateWidget->setFixedWidth(GUIUtil::DATE_COLUMN_WIDTH-1);
    }
    dateWidget->addItem(tr("All"), All);
    dateWidget->addItem(tr("Today"), Today);
    dateWidget->addItem(tr("This week"), ThisWeek);
    dateWidget->addItem(tr("This month"), ThisMonth);
    dateWidget->addItem(tr("Last month"), LastMonth);
    dateWidget->addItem(tr("This year"), ThisYear);
    dateWidget->addItem(tr("Range..."), Range);
    dateWidget->setCurrentIndex(settings.value("transactionDate").toInt());
    hlayout->addWidget(dateWidget);

    assetsNameWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    assetsNameWidget->setPlaceholderText(tr("Enter assets name to search"));
#endif
    assetsNameWidget->setObjectName("assetsNameWidget");
    if (platformStyle->getUseExtraSpacing()) {
        assetsNameWidget->setFixedWidth(GUIUtil::ASSETS_NAME_COLUMN_WIDTH);
    } else {
        assetsNameWidget->setFixedWidth(GUIUtil::ASSETS_NAME_COLUMN_WIDTH-1);
    }
    hlayout->addWidget(assetsNameWidget);

    addressWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    addressWidget->setPlaceholderText(tr("Enter address or label to search"));
#endif
    addressWidget->setObjectName("addressWidget");
    hlayout->addWidget(addressWidget);

    amountWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    amountWidget->setPlaceholderText(tr("Min amount"));
    amountWidget->setAlignment(Qt::AlignRight);
#endif
    if (platformStyle->getUseExtraSpacing()) {
        amountWidget->setFixedWidth(GUIUtil::AMOUNT_MINIMUM_COLUMN_WIDTH);
    } else {
        amountWidget->setFixedWidth(GUIUtil::AMOUNT_MINIMUM_COLUMN_WIDTH-1);
    }
    amountWidget->setValidator(new CustomDoubleValidator(0, gFilterAmountMaxNum, 8, this));
    amountWidget->setObjectName("amountWidget");
    hlayout->addWidget(amountWidget);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0,0,0,0);
    vlayout->setSpacing(0);

    QTableView *view = new QTableView(this);
    vlayout->addLayout(hlayout);
    vlayout->addWidget(createDateRangeWidget());
    vlayout->addWidget(view);
    vlayout->setSpacing(0);
    int width = view->verticalScrollBar()->sizeHint().width();
    // Cover scroll bar width with spacing
    if (platformStyle->getUseExtraSpacing()) {
        hlayout->addSpacing(width+2);
    } else {
        hlayout->addSpacing(width);
    }
    // Always show scroll bar
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);

    view->installEventFilter(this);

    candyView = view;

    // Actions
    abandonAction = new QAction(tr("Abandon transaction"), this);
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
    QAction *copyAssetName = new QAction(tr("Copy asset name"), this);
    QAction *copyTxIDAction = new QAction(tr("Copy transaction ID"), this);
    QAction *copyTxHexAction = new QAction(tr("Copy raw transaction"), this);
    QAction *copyTxPlainText = new QAction(tr("Copy full transaction details"), this);
    QAction *editLabelAction = new QAction(tr("Edit label"), this);
    QAction *showDetailsAction = new QAction(tr("Show transaction details"), this);

    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyAssetName);
    contextMenu->addAction(copyTxIDAction);
    contextMenu->addAction(copyTxHexAction);
    contextMenu->addAction(copyTxPlainText);
    contextMenu->addAction(showDetailsAction);
    contextMenu->addSeparator();
    contextMenu->addAction(abandonAction);
    contextMenu->addAction(editLabelAction);
    contextMenu->setStyleSheet("font-size:12px;");
    mapperThirdPartyTxUrls = new QSignalMapper(this);

    // Connect actions
    connect(mapperThirdPartyTxUrls, SIGNAL(mapped(QString)), this, SLOT(openThirdPartyTxUrl(QString)));
    connect(assetsNameWidget,SIGNAL(textChanged(QString)), this, SLOT(changedAssetsName(QString)));
    connect(dateWidget, SIGNAL(activated(int)), this, SLOT(chooseDate(int)));
    connect(watchOnlyWidget, SIGNAL(activated(int)), this, SLOT(chooseWatchonly(int)));
    connect(addressWidget, SIGNAL(textChanged(QString)), this, SLOT(changedPrefix(QString)));
    connect(amountWidget, SIGNAL(textChanged(QString)), this, SLOT(changedAmount(QString)));

    connect(view, SIGNAL(doubleClicked(QModelIndex)), this, SIGNAL(doubleClicked(QModelIndex)));
    connect(view, SIGNAL(clicked(QModelIndex)), this, SLOT(computeSum()));
    connect(view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    connect(abandonAction, SIGNAL(triggered()), this, SLOT(abandonTx()));
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(copyAssetName,SIGNAL(triggered()),this,SLOT(copyAssetName()));
    connect(copyTxIDAction, SIGNAL(triggered()), this, SLOT(copyTxID()));
    connect(copyTxHexAction, SIGNAL(triggered()), this, SLOT(copyTxHex()));
    connect(copyTxPlainText, SIGNAL(triggered()), this, SLOT(copyTxPlainText()));
    connect(editLabelAction, SIGNAL(triggered()), this, SLOT(editLabel()));
    connect(showDetailsAction, SIGNAL(triggered()), this, SLOT(showDetails()));
}

void CandyView::setModel(WalletModel *model)
{
    QSettings settings;
    this->model = model;
    if(model)
    {
        transactionProxyModel = new TransactionFilterProxy(this);
        transactionProxyModel->setSourceModel(model->getCandyTableModel());
        transactionProxyModel->setDynamicSortFilter(true);
        transactionProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        transactionProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        transactionProxyModel->setFilterType(false);

        transactionProxyModel->setSortRole(Qt::EditRole);

        candyView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        candyView->setModel(transactionProxyModel);
        candyView->setAlternatingRowColors(true);
        candyView->setSelectionBehavior(QAbstractItemView::SelectRows);
        candyView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        candyView->setSortingEnabled(true);
        candyView->sortByColumn(CandyTableModel::CandyColumnStatus, Qt::DescendingOrder);
        candyView->verticalHeader()->hide();

        candyView->setColumnWidth(CandyTableModel::CandyColumnStatus, GUIUtil::STATUS_COLUMN_WIDTH);
        candyView->setColumnWidth(CandyTableModel::CandyColumnWatchonly, GUIUtil::WATCHONLY_COLUMN_WIDTH);
        candyView->setColumnWidth(CandyTableModel::CandyColumnDate, GUIUtil::DATE_COLUMN_WIDTH);
        candyView->setColumnWidth(CandyTableModel::CandyColumnAssetsName, GUIUtil::ASSETS_NAME_COLUMN_WIDTH);
        if (useExtraSpacing) {
            candyView->setColumnWidth(CandyTableModel::CandyColumnAmount, GUIUtil::AMOUNT_MINIMUM_COLUMN_WIDTH);
        }else{
            candyView->setColumnWidth(CandyTableModel::CandyColumnAmount, GUIUtil::AMOUNT_MINIMUM_COLUMN_WIDTH-1);
        }
        candyView->setStyleSheet("QTableView{padding-left:5px;}");

        // Note: it's a good idea to connect this signal AFTER the model is set
        connect(candyView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(computeSum()));

        columnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(candyView, GUIUtil::AMOUNT_MINIMUM_COLUMN_WIDTH, GUIUtil::MINIMUM_COLUMN_WIDTH, this);

        if (model->getOptionsModel())
        {
            // Add third party transaction URLs to context menu
            QStringList listUrls = model->getOptionsModel()->getThirdPartyTxUrls().split("|", QString::SkipEmptyParts);
            for (int i = 0; i < listUrls.size(); ++i)
            {
                QString host = QUrl(listUrls[i].trimmed(), QUrl::StrictMode).host();
                if (!host.isEmpty())
                {
                    QAction *thirdPartyTxUrlAction = new QAction(host, this); // use host as menu item label
                    if (i == 0)
                        contextMenu->addSeparator();
                    contextMenu->addAction(thirdPartyTxUrlAction);
                    connect(thirdPartyTxUrlAction, SIGNAL(triggered()), mapperThirdPartyTxUrls, SLOT(map()));
                    mapperThirdPartyTxUrls->setMapping(thirdPartyTxUrlAction, listUrls[i].trimmed());
                }
            }
        }

        // show/hide column Watch-only
        updateWatchOnlyColumn(model->haveWatchOnly());

        // Watch-only signal
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyColumn(bool)));

        // Update transaction list with persisted settings
        chooseDate(settings.value("transactionDate").toInt());
    }
}

void CandyView::chooseDate(int idx)
{
    if(!transactionProxyModel)
        return;

    QSettings settings;
    QDate current = QDate::currentDate();
    dateRangeWidget->setVisible(false);
    switch(dateWidget->itemData(idx).toInt())
    {
    case All:
        transactionProxyModel->setDateRange(
                TransactionFilterProxy::MIN_DATE,
                TransactionFilterProxy::MAX_DATE);
        break;
    case Today:
        transactionProxyModel->setDateRange(
                QDateTime(current),
                TransactionFilterProxy::MAX_DATE);
        break;
    case ThisWeek: {
        // Find last Monday
        QDate startOfWeek = current.addDays(-(current.dayOfWeek()-1));
        transactionProxyModel->setDateRange(
                QDateTime(startOfWeek),
                TransactionFilterProxy::MAX_DATE);

        } break;
    case ThisMonth:
        transactionProxyModel->setDateRange(
                QDateTime(QDate(current.year(), current.month(), 1)),
                TransactionFilterProxy::MAX_DATE);
        break;
    case LastMonth:
        transactionProxyModel->setDateRange(
                QDateTime(QDate(current.year(), current.month(), 1).addMonths(-1)),
                QDateTime(QDate(current.year(), current.month(), 1)));
        break;
    case ThisYear:
        transactionProxyModel->setDateRange(
                QDateTime(QDate(current.year(), 1, 1)),
                TransactionFilterProxy::MAX_DATE);
        break;
    case Range:
        dateRangeWidget->setVisible(true);
        dateRangeChanged();
        break;
    }
    // Persist new date settings
    settings.setValue("transactionDate", idx);
    if (dateWidget->itemData(idx).toInt() == Range){
        settings.setValue("transactionDateFrom", dateFrom->date().toString(PERSISTENCE_DATE_FORMAT));
        settings.setValue("transactionDateTo", dateTo->date().toString(PERSISTENCE_DATE_FORMAT));
    }
}

void CandyView::changedAssetsName(const QString &assetsName)
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setAssetsNamePrefix(assetsName);
}

void CandyView::chooseWatchonly(int idx)
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setWatchOnlyFilter(
        (TransactionFilterProxy::WatchOnlyFilter)watchOnlyWidget->itemData(idx).toInt());
}

void CandyView::changedPrefix(const QString &prefix)
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setAddressPrefix(prefix);
}

void CandyView::changedAmount(const QString &amount)
{
    if(!transactionProxyModel)
        return;

    QString amountStr = amount;
    if(amountStr.endsWith('.')){
        int lastIndex = amountStr.length() - 1;
        amountStr.remove(lastIndex,1);
    }
    transactionProxyModel->setMinAssetsAmountStr(amountStr);

//    CAmount amount_parsed = 0;

//    // Replace "," by "." so BitcoinUnits::parse will not fail for users entering "," as decimal separator
//    QString newAmount = amount;
//    newAmount.replace(QString(","), QString("."));

//    if(BitcoinUnits::parse(model->getOptionsModel()->getDisplayUnit(), newAmount, &amount_parsed))
//    {
//        transactionProxyModel->setMinAmount(amount_parsed);
//    }
//    else
//    {
//        transactionProxyModel->setMinAmount(0);
//    }
}

void CandyView::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Export Get Candy History"), QString(),
        tr("Comma separated file (*.csv)"), NULL);

    if (filename.isNull())
        return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(transactionProxyModel);
    writer.addColumn(tr("Confirmed"), 0, TransactionTableModel::ConfirmedRole);
    if (model && model->haveWatchOnly())
        writer.addColumn(tr("Watch-only"), TransactionTableModel::WatchonlyRole);
    writer.addColumn(tr("Date"), 0, TransactionTableModel::DateRole);
    writer.addColumn(tr("Asset Name"), 0, TransactionTableModel::AssetsNameRole);
    writer.addColumn(tr("Address"), 0, TransactionTableModel::AddressRole);
    writer.addColumn(BitcoinUnits::getAmountColumnTitle(model->getOptionsModel()->getDisplayUnit()), 0, TransactionTableModel::FormattedAmountRole);
    writer.addColumn(tr("ID"), 0, TransactionTableModel::TxIDRole);

    if(!writer.write()) {
        Q_EMIT message(tr("Exporting Failed"), tr("There was an error trying to save the get candy history to %1.").arg(filename),
            CClientUIInterface::MSG_ERROR);
    }
    else {
        Q_EMIT message(tr("Exporting Successful"), tr("The get candy history was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void CandyView::contextualMenu(const QPoint &point)
{
    QModelIndex index = candyView->indexAt(point);
    if(!index.isValid())
        return;
    QModelIndexList selection = candyView->selectionModel()->selectedRows(0);

    // check if transaction can be abandoned, disable context menu action in case it doesn't
    uint256 hash;
    hash.SetHex(selection.at(0).data(TransactionTableModel::TxHashRole).toString().toStdString());
    abandonAction->setEnabled(model->transactionCanBeAbandoned(hash));
    contextMenu->exec(QCursor::pos());
}

void CandyView::abandonTx()
{
    if(!candyView || !candyView->selectionModel())
        return;
    QModelIndexList selection = candyView->selectionModel()->selectedRows(0);

    // get the hash from the TxHashRole (QVariant / QString)
    uint256 hash;
    QString hashQStr = selection.at(0).data(TransactionTableModel::TxHashRole).toString();
    hash.SetHex(hashQStr.toStdString());

    // Abandon the wallet transaction over the walletModel
    model->abandonTransaction(hash);

    // Update the table
    model->getCandyTableModel()->updateTransaction(hashQStr, CT_UPDATED, false);
}

void CandyView::copyAddress()
{
    GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::AddressRole);
}

void CandyView::copyLabel()
{
    GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::LabelRole);
}

void CandyView::copyAmount()
{
    GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::FormattedAmountRole);
}

void CandyView::copyAssetName()
{
    GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::AssetsNameRole);
}

void CandyView::copyTxID()
{
    GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::TxIDRole);
}

void CandyView::copyTxHex()
{
    GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::TxHexRole);
}

void CandyView::copyTxPlainText()
{
    GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::TxPlainTextRole);
}

void CandyView::editLabel()
{
    if(!candyView->selectionModel() ||!model)
        return;
    QModelIndexList selection = candyView->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        AddressTableModel *addressBook = model->getAddressTableModel();
        if(!addressBook)
            return;
        QString address = selection.at(0).data(TransactionTableModel::AddressRole).toString();
        if(address.isEmpty())
        {
            // If this transaction has no associated address, exit
            return;
        }
        // Is address in address book? Address book can miss address when a transaction is
        // sent from outside the UI.
        int idx = addressBook->lookupAddress(address);
        if(idx != -1)
        {
            // Edit sending / receiving address
            QModelIndex modelIdx = addressBook->index(idx, 0, QModelIndex());
            // Determine type of address, launch appropriate editor dialog type
            QString type = modelIdx.data(AddressTableModel::TypeRole).toString();

            EditAddressDialog dlg(
                type == AddressTableModel::Receive
                ? EditAddressDialog::EditReceivingAddress
                : EditAddressDialog::EditSendingAddress, this);
            dlg.setModel(addressBook);
            dlg.loadRow(idx);
            dlg.exec();
        }
        else
        {
            // Add sending address
            EditAddressDialog dlg(EditAddressDialog::NewSendingAddress,
                this);
            dlg.setModel(addressBook);
            dlg.setAddress(address);
            dlg.exec();
        }
    }
}

void CandyView::showDetails()
{
    if(!candyView->selectionModel())
        return;
    QModelIndexList selection = candyView->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        TransactionDescDialog dlg(selection.at(0));
        dlg.exec();
    }
}

/** Compute sum of all selected transactions */
void CandyView::computeSum()
{
    if(!candyView->selectionModel())
        return;
    QModelIndexList selection = candyView->selectionModel()->selectedRows();
    if(selection.empty())
        return;

    qint64 amount = 0;
    QString strUnit = "";
    QString firstAssetName = "";
    int i = 0;
    int assetsDecimal = 0;
    Q_FOREACH (QModelIndex index, selection)
    {
        if(!index.isValid())
            continue;
        i++;
        if(i==1)
        {
            amount += index.data(TransactionTableModel::AssetsAmountRole).toLongLong();
            strUnit = index.data(TransactionTableModel::AmountUnitRole).toString();
            assetsDecimal = index.data(TransactionTableModel::AssetsDecimalsRole).toInt();
            firstAssetName = index.data(TransactionTableModel::AssetsNameRole).toString();
        }else
        {
            QString currAmountName = index.data(TransactionTableModel::AssetsNameRole).toString();
            if(firstAssetName==currAmountName)
                amount += index.data(TransactionTableModel::AssetsAmountRole).toLongLong();
        }
    }

    QString strAmount(BitcoinUnits::formatWithUnit(assetsDecimal, amount, true, BitcoinUnits::separatorAlways,true,strUnit));
    if (amount < 0)
        strAmount = "<span style='color:red;'>" + strAmount + "</span>";
    Q_EMIT trxAmount(strAmount);
}

void CandyView::openThirdPartyTxUrl(QString url)
{
    if(!candyView || !candyView->selectionModel())
        return;
    QModelIndexList selection = candyView->selectionModel()->selectedRows(0);
    if(!selection.isEmpty())
         QDesktopServices::openUrl(QUrl::fromUserInput(url.replace("%s", selection.at(0).data(TransactionTableModel::TxHashRole).toString())));
}

QWidget *CandyView::createDateRangeWidget()
{
    // Create default dates in case nothing is persisted
    QString defaultDateFrom = QDate::currentDate().toString(PERSISTENCE_DATE_FORMAT);
    QString defaultDateTo = QDate::currentDate().addDays(1).toString(PERSISTENCE_DATE_FORMAT);
    QSettings settings;

    dateRangeWidget = new QFrame();
    dateRangeWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    dateRangeWidget->setContentsMargins(1,1,1,1);
    QHBoxLayout *layout = new QHBoxLayout(dateRangeWidget);
    layout->setContentsMargins(0,0,0,0);
    layout->addSpacing(23);
    layout->addWidget(new QLabel(tr("Range:")));

    dateFrom = new QDateTimeEdit(this);
    dateFrom->setCalendarPopup(true);
    dateFrom->setMinimumWidth(100);
    // Load persisted FROM date
    dateFrom->setDate(QDate::fromString(settings.value("transactionDateFrom", defaultDateFrom).toString(), PERSISTENCE_DATE_FORMAT));

    layout->addWidget(dateFrom);
    layout->addWidget(new QLabel(tr("to")));

    dateTo = new QDateTimeEdit(this);
    dateTo->setCalendarPopup(true);
    dateTo->setMinimumWidth(100);
    // Load persisted TO date
    dateTo->setDate(QDate::fromString(settings.value("transactionDateTo", defaultDateTo).toString(), PERSISTENCE_DATE_FORMAT));

    layout->addWidget(dateTo);
    layout->addStretch();

    // Hide by default
    dateRangeWidget->setVisible(false);

    // Notify on change
    connect(dateFrom, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));
    connect(dateTo, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));

    return dateRangeWidget;
}

void CandyView::dateRangeChanged()
{
    if(!transactionProxyModel)
        return;

    // Persist new date range
    QSettings settings;
    settings.setValue("transactionDateFrom", dateFrom->date().toString(PERSISTENCE_DATE_FORMAT));
    settings.setValue("transactionDateTo", dateTo->date().toString(PERSISTENCE_DATE_FORMAT));

    transactionProxyModel->setDateRange(
            QDateTime(dateFrom->date()),
            QDateTime(dateTo->date()));
}

void CandyView::focusTransaction(const QModelIndex &idx)
{
    if(!transactionProxyModel)
        return;
    QModelIndex targetIdx = transactionProxyModel->mapFromSource(idx);
    candyView->selectRow(targetIdx.row());
    computeSum();
    candyView->scrollTo(targetIdx);
    candyView->setCurrentIndex(targetIdx);
    candyView->setFocus();
}

// We override the virtual resizeEvent of the QWidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void CandyView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    columnResizingFixer->stretchColumnWidth(CandyTableModel::CandyColumnToAddress);
}

 //Need to override default Ctrl+C action for amount as default behaviour is just to copy DisplayRole text
bool CandyView::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_C && ke->modifiers().testFlag(Qt::ControlModifier))
        {
             GUIUtil::copyEntryData(candyView, 0, TransactionTableModel::TxPlainTextRole);
             return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// show/hide column Watch-only
void CandyView::updateWatchOnlyColumn(bool fHaveWatchOnly)
{
    watchOnlyWidget->setVisible(true);
    candyView->setColumnHidden(CandyTableModel::CandyColumnWatchonly, !fHaveWatchOnly);
}
