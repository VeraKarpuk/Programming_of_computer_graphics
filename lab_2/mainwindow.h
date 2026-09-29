#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <QString>
#include "imagemetadata.h"

class QTableView;
class QProgressBar;
class QPushButton;
class QLabel;
class QStandardItemModel;
class QSortFilterProxyModel;
class QLineEdit;
class ScanManager;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void selectFolder();
    void addResult(const ImageMetadata &meta);
    void updateProgress(int current, int total);
    void onFinished();
    void onRowSelected(const QModelIndex &current);
    void onFilterChanged(const QString &text);
    void clearResults();

private:
    void setupUi();

    QTableView *m_tableView = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton *m_btnSelect = nullptr;
    QPushButton *m_btnClear = nullptr;
    QLabel *m_lblStats = nullptr;
    QLabel *m_preview = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    ScanManager *m_manager = nullptr;
    QElapsedTimer m_elapsed;
    QString m_lastDir;
};

#endif