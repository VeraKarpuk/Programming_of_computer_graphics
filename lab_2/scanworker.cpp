#include "scanworker.h"
#include <QFile>
#include <QFileInfo>
#include "parserfactory.h"

ScanWorker::ScanWorker(const QString &filePath)
    : m_filePath(filePath) {
    setAutoDelete(true);
}

void ScanWorker::run() {
    ImageMetadata meta;
    QFileInfo fi(m_filePath);
    meta.fileName = fi.fileName();
    meta.status = "OK";
    meta.resolution = 0.0;
    meta.bitDepth = 0;
    meta.size = QSize(0, 0);

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        meta.status = "Error";
        meta.errorMessage = "Cannot open file";
        emit resultReady(meta);
        return;
    }

    if (file.size() == 0) {
        meta.status = "Corrupted";
        meta.errorMessage = "File is empty";
        file.close();
        emit resultReady(meta);
        return;
    }

    if (file.size() < 16) {
        meta.status = "Corrupted";
        meta.errorMessage = "File too small";
        file.close();
        emit resultReady(meta);
        return;
    }

    auto parser = ParserFactory::createParser(m_filePath);
    if (!parser) {
        meta.status = "Unsupported";
        meta.errorMessage = "Unknown format";
    } else {
        try {
            if (!parser->parse(file, meta)) {
                meta.status = "Corrupted";
                if (meta.errorMessage.isEmpty())
                    meta.errorMessage = "Parse failed";
            }
        } catch (const std::exception &e) {
            meta.status = "Corrupted";
            meta.errorMessage = QString("Exception: %1").arg(e.what());
        } catch (...) {
            meta.status = "Corrupted";
            meta.errorMessage = "Unknown exception";
        }
    }

    file.close();

    if (meta.status == "OK") {
        if (meta.size.width() <= 0 || meta.size.height() <= 0) {
            meta.status = "Corrupted";
            meta.errorMessage = "Invalid dimensions";
        } else if (meta.size.width() > 100000 || meta.size.height() > 100000) {
            meta.status = "Corrupted";
            meta.errorMessage = "Dimensions out of range";
        } else if (meta.resolution <= 0.0 || meta.resolution > 100000.0) {
            meta.resolution = 72.0;
        }
    }

    emit resultReady(meta);
}