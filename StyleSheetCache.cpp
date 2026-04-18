#include "StyleSheetCache.h"

QHash<QString, QString> StyleSheetCache::s_styleTemplateCache;
QHash<QString, QString> StyleSheetCache::s_styleCache;
bool StyleSheetCache::s_initialized = false;
