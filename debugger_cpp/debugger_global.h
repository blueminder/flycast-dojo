#ifndef DEBUGGER_GLOBAL_H
#define DEBUGGER_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(DEBUGGER_LIBRARY)
#  define DEBUGGER_EXPORT Q_DECL_EXPORT
#else
#  define DEBUGGER_EXPORT Q_DECL_IMPORT
#endif

#endif // DEBUGGER_GLOBAL_H
