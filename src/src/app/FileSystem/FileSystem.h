#pragma once

#include <Arduino.h>
#include <FS.h>

using fs::File;

// Common interface over whatever storage the board has, so the rest of the
// firmware never names a concrete filesystem.
class FileSystem
{
public:
    virtual bool begin() = 0;
    virtual void end() {};
    virtual File open(const char *path, const char *mode) = 0;
    virtual bool exists(const char *path) = 0;
    virtual bool remove(const char *path) = 0;
    virtual bool rename(const char *pathFrom, const char *pathTo) = 0;
    virtual ~FileSystem() = default;
};
