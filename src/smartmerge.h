#ifndef SMARTMERGE_H
#define SMARTMERGE_H

#include <QString>

namespace SmartMerge {

bool preparePartialWrite(const QString &readbackPath,
                         const QString &inputPath,
                         const QString &mergedPath,
                         const QString &layoutPath,
                         qint64 imageSize,
                         QString *errorMessage);

}

#endif // SMARTMERGE_H
