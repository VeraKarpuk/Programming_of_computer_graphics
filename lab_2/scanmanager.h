#ifndef SCANMANAGER_H
#define SCANMANAGER_H

#include <QObject>
#include <QThreadPool>
#include <QAtomicInt>
#include "imagemetadata.h"

class ScanManager : public QObject {
    Q_OBJECT
public:
    explicit ScanManager(QObject *parent = nullptr);
    void startScan(const QString &dirPath);
    void stopScan();

signals:
    void progressChanged(int current, int total);
    void resultReady(const ImageMetadata &meta);
    void finished();

private slots:
    void handleResult(const ImageMetadata &meta);

private:
    QThreadPool m_threadPool;
    QAtomicInt m_totalFiles{0};
    QAtomicInt m_processedFiles{0};
    QAtomicInt m_stopFlag{0};
};

#endif