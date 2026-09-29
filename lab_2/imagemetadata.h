#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QString>
#include <QSize>
#include <QMetaType>

struct ImageMetadata {
    QString fileName;
    QString format;
    QSize size;
    double resolution;
    int bitDepth;
    QString compression;
    QString status;
    QString errorMessage;
};

Q_DECLARE_METATYPE(ImageMetadata)

#endif