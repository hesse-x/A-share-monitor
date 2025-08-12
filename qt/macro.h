#ifndef MACRO_H
#define MACRO_H
#include <QtGlobal>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#define QT6_OR_NEWER
#else
#define QT5_OR_OLDER
#endif
#endif // MACRO_H
