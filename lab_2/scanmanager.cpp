#include "scanmanager.h"
#include <QDir>
#include <QFileInfoList>
#include <QThread>
#include "scanworker.h"

ScanManager::ScanManager(QObject *parent) : QObject(parent) {
    int threads = QThread::idealThreadCount();
    if (threads < 2) threads = 2;
    m_threadPool.setMaxThreadCount(threads);
}

void ScanManager::startScan(const QString &dirPath) {
    m_stopFlag.storeRelaxed(0);
    m_processedFiles.storeRelaxed(0);

    QDir dir(dirPath);
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp"
            << "*.gif" << "*.tif" << "*.tiff" << "*.pcx";
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoSymLinks);

    QFileInfoList files = dir.entryInfoList();
    m_totalFiles.storeRelaxed(files.size());

    emit progressChanged(0, files.size());

    if (files.isEmpty()) {
        emit finished();
        return;
    }

    for (const QFileInfo &fi : files) {
        if (m_stopFlag.loadRelaxed()) break;
        ScanWorker *worker = new ScanWorker(fi.absoluteFilePath());
        connect(worker, &ScanWorker::resultReady,
                this, &ScanManager::handleResult,
                Qt::QueuedConnection);
        m_threadPool.start(worker);
    }
}

void ScanManager::stopScan() {
    m_stopFlag.storeRelaxed(1);
    m_threadPool.clear();
}

void ScanManager::handleResult(const ImageMetadata &meta) {
    emit resultReady(meta);
    int done = m_processedFiles.fetchAndAddRelaxed(1) + 1;
    emit progressChanged(done, m_totalFiles.loadRelaxed());
    if (done >= m_totalFiles.loadRelaxed()) {
        emit finished();
    }
}