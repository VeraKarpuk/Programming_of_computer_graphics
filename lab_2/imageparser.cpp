#include "imageparser.h"

quint8 ImageParser::readU8(QFile &file) {
    char c;
    file.read(&c, 1);
    return static_cast<quint8>(c);
}

quint16 ImageParser::readU16(QFile &file, bool bigEndian) {
    unsigned char b[2];
    file.read(reinterpret_cast<char*>(b), 2);
    if (bigEndian)
        return (quint16(b[0]) << 8) | b[1];
    return (quint16(b[1]) << 8) | b[0];
}

quint32 ImageParser::readU32(QFile &file, bool bigEndian) {
    unsigned char b[4];
    file.read(reinterpret_cast<char*>(b), 4);
    if (bigEndian)
        return (quint32(b[0]) << 24) | (quint32(b[1]) << 16)
               | (quint32(b[2]) << 8)  | b[3];
    return (quint32(b[3]) << 24) | (quint32(b[2]) << 16)
           | (quint32(b[1]) << 8)  | b[0];
}