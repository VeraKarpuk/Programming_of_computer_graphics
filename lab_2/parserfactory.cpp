#include "parserfactory.h"
#include <QFile>
#include "pngparser.h"
#include "jpegparser.h"
#include "bmpparser.h"
#include "gifparser.h"
#include "tiffparser.h"
#include "pcxparser.h"

QSharedPointer<ImageParser> ParserFactory::createParser(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;
    QByteArray magic = file.read(4);
    file.close();

    if (magic.startsWith("\x89PNG"))
        return QSharedPointer<ImageParser>(new PngParser());
    if (magic.startsWith("\xFF\xD8"))
        return QSharedPointer<ImageParser>(new JpegParser());
    if (magic.startsWith("BM"))
        return QSharedPointer<ImageParser>(new BmpParser());
    if (magic.startsWith("GIF8"))
        return QSharedPointer<ImageParser>(new GifParser());
    if (magic.startsWith("II\x2A\x00") || magic.startsWith("MM\x00\x2A"))
        return QSharedPointer<ImageParser>(new TiffParser());
    if (magic.startsWith("\x0A"))
        return QSharedPointer<ImageParser>(new PcxParser());
    return nullptr;
}