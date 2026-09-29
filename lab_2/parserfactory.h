#ifndef PARSERFACTORY_H
#define PARSERFACTORY_H

#include <QSharedPointer>
#include <QString>
#include "imageparser.h"

class ParserFactory {
public:
    static QSharedPointer<ImageParser> createParser(const QString &filePath);
};

#endif