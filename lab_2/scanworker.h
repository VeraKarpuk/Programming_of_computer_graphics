#ifndef SCANWORKER_H
#define SCANWORKER_H

#include <QObject>
#include <QRunnable>
#include <QString>
#include "imagemetadata.h"

class ScanWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit ScanWorker(const QString &filePath);
    void run() override;

signals:
    void resultReady(const ImageMetadata &meta);

private:
    QString m_filePath;
};

#endif