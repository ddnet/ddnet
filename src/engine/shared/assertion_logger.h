#ifndef ENGINE_SHARED_ASSERTION_LOGGER_H
#define ENGINE_SHARED_ASSERTION_LOGGER_H

#include <memory>

class ILogger;
class IStorage;

std::unique_ptr<ILogger> CreateAssertionLogger(IStorage *pStorage, const char *pGameName);

#endif
