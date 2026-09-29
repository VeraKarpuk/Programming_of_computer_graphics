#include "mainwindow.h"
#include <QTableView>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QPixmap>
#include <QSplitter>
#include <QItemSelectionModel>
#include "scanmanager.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({
        "File Name", "Format", "Size (px)", "DPI",
        "Bit Depth", "Compression", "Status"
    });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    m_tableView->setModel(m_proxy);
    m_tableView->setSortingEnabled(true);

    connect(m_tableView->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::onRowSelected);

    m_manager = new ScanManager(this);
    connect(m_manager, &ScanManager::resultReady,
            this, &MainWindow::addResult, Qt::QueuedConnection);
    connect(m_manager, &ScanManager::progressChanged,
            this, &MainWindow::updateProgress, Qt::QueuedConnection);
    connect(m_manager, &ScanManager::finished,
            this, &MainWindow::onFinished, Qt::QueuedConnection);
}

MainWindow::~MainWindow() = default;

void MainWindow::selectFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Folder", m_lastDir);
    if (dir.isEmpty()) return;

    m_lastDir = dir;
    m_model->removeRows(0, m_model->rowCount());
    m_preview->setText("Preview");
    m_preview->setPixmap(QPixmap());
    m_btnSelect->setEnabled(false);
    m_btnClear->setEnabled(false);
    m_elapsed.start();
    m_manager->startScan(dir);
}

void MainWindow::clearResults() {
    m_model->removeRows(0, m_model->rowCount());
    m_preview->setText("Preview");
    m_preview->setPixmap(QPixmap());
    m_lblStats->setText("Ready");
    m_progressBar->setValue(0);
}

void MainWindow::addResult(const ImageMetadata &meta) {
    QList<QStandardItem*> row;
    row << new QStandardItem(meta.fileName);
    row << new QStandardItem(meta.format);
    row << new QStandardItem(QString("%1 x %2")
                                 .arg(meta.size.width()).arg(meta.size.height()));
    row << new QStandardItem(QString::number(meta.resolution, 'f', 1));
    row << new QStandardItem(QString::number(meta.bitDepth));
    row << new QStandardItem(meta.compression);
    row << new QStandardItem(meta.status);

    for (auto *item : row)
        item->setEditable(false);

    if (meta.status == "Corrupted") {
        row[6]->setForeground(QBrush(Qt::red));
        row[6]->setToolTip(meta.errorMessage);
        row[0]->setToolTip(meta.errorMessage);
    } else if (meta.status == "Unsupported") {
        row[6]->setForeground(QBrush(Qt::darkYellow));
        row[6]->setToolTip(meta.errorMessage);
    } else if (meta.status == "Error") {
        row[6]->setForeground(QBrush(Qt::magenta));
        row[6]->setToolTip(meta.errorMessage);
    } else {
        row[6]->setForeground(QBrush(Qt::darkGreen));
    }

    m_model->appendRow(row);
}

void MainWindow::updateProgress(int current, int total) {
    m_progressBar->setMaximum(total > 0 ? total : 1);
    m_progressBar->setValue(current);
    m_lblStats->setText(QString("Processed: %1 / %2").arg(current).arg(total));
}

void MainWindow::onFinished() {
    m_btnSelect->setEnabled(true);
    m_btnClear->setEnabled(true);
    qint64 ms = m_elapsed.elapsed();
    double sec = ms / 1000.0;
    m_lblStats->setText(QString("Done. Files: %1. Time: %2 s (%3 ms)")
                            .arg(m_model->rowCount()).arg(sec, 0, 'f', 2).arg(ms));
}

void MainWindow::onRowSelected(const QModelIndex &current) {
    if (!current.isValid()) return;

    QModelIndex srcIndex = m_proxy->mapToSource(current);
    QString fileName = m_model->item(srcIndex.row(), 0)->text();
    QString fullPath = QDir(m_lastDir).filePath(fileName);

    QPixmap pix(fullPath);
    if (pix.isNull()) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText("Preview unavailable\n(file corrupted or unsupported)");
        return;
    }
    m_preview->setPixmap(pix.scaled(m_preview->size(),
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation));
}

void MainWindow::onFilterChanged(const QString &text) {
    m_proxy->setFilterFixedString(text);
}

void MainWindow::setupUi() {
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    QHBoxLayout *topLayout = new QHBoxLayout();

    m_btnSelect = new QPushButton("Select Folder", this);
    connect(m_btnSelect, &QPushButton::clicked,
            this, &MainWindow::selectFolder);

    m_btnClear = new QPushButton("Clear", this);
    m_btnClear->setEnabled(false);
    connect(m_btnClear, &QPushButton::clicked,
            this, &MainWindow::clearResults);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter (file name, format, status...)");
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onFilterChanged);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setValue(0);

    m_lblStats = new QLabel("Ready", this);

    topLayout->addWidget(m_btnSelect);
    topLayout->addWidget(m_btnClear);
    topLayout->addWidget(m_filterEdit, 2);
    topLayout->addWidget(m_progressBar, 2);
    topLayout->addWidget(m_lblStats);

    m_tableView = new QTableView(this);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->verticalHeader()->setVisible(false);

    m_preview = new QLabel("Preview", this);
    m_preview->setMinimumWidth(320);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet("border: 1px solid gray; background: #f0f0f0;");
    m_preview->setWordWrap(true);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_tableView);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    layout->addLayout(topLayout);
    layout->addWidget(splitter);
    setCentralWidget(central);
    resize(1300, 750);
    setWindowTitle("Image Metadata Scanner");
}