#ifndef PCXPARSER_H
#define PCXPARSER_H

#include "imageparser.h"

class PcxParser : public ImageParser {
public:
    bool parse(QFile &file, ImageMetadata &meta) override;
};

#endif