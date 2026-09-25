#pragma once

#include "app/FileSystem/FileSystem.h"

#include <Arduino.h>
#include <FS.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory>

#include "lfs.h"

class LfsFSImpl;

// littlefs on a raw flash partition, erased in whole blocks of blockSize.
//
// The board's S25FL128S only honours 4 KB erases in its bottom parameter
// area; everywhere else the smallest erase is a 64 KB block. FAT and the
// stock LittleFS both assume 4 KB sectors, so their erases silently do
// nothing and writes come back corrupt. littlefs itself takes the erase
// block size as a parameter, so this mounts it directly with 64 KB blocks.
//
// 64 KB is also a valid erase on ordinary 4 KB flash, so the same code runs
// on any ESP32 board. Parts with uniform 256 KB sectors (S25FL512S and
// similar) need blockSize = 256 KB. The partition's offset and size must be
// multiples of blockSize.
//
// Files come back as plain Arduino File objects, and fs() hands out an
// fs::FS for APIs that want one, such as WebServer::serveStatic.
class FileSystemLFS : public FileSystem
{
public:
    FileSystemLFS(
        const char *partitionLabel = "storage",
        uint32_t blockSize = 64 * 1024,
        bool formatOnFail = true);
    ~FileSystemLFS() override;

    bool begin() override;
    void end() override;
    bool mounted() const { return _mounted; }

    // erases everything on the partition
    bool format();

    File open(const char *path, const char *mode) override;
    bool exists(const char *path) override;
    bool remove(const char *path) override;
    bool rename(const char *pathFrom, const char *pathTo) override;

    // true if the directory exists afterwards, whether or not it was created
    bool mkdir(const char *path);
    File openDir(const char *path);

    size_t totalBytes();
    size_t usedBytes();
    size_t freeBytes();

    fs::FS &fs() { return _fs; }

private:
    static int flashRead(const struct lfs_config *c, lfs_block_t block,
                         lfs_off_t off, void *buffer, lfs_size_t size);
    static int flashProg(const struct lfs_config *c, lfs_block_t block,
                         lfs_off_t off, const void *buffer, lfs_size_t size);
    static int flashErase(const struct lfs_config *c, lfs_block_t block);
    static int flashSync(const struct lfs_config *c);
    static int lock(const struct lfs_config *c);
    static int unlock(const struct lfs_config *c);

    bool configure();

    const char *_partitionLabel;
    uint32_t _blockSize;
    bool _formatOnFail;
    bool _mounted;

    const esp_partition_t *_partition;
    SemaphoreHandle_t _mutex;
    lfs_t _lfs;
    struct lfs_config _cfg;

    std::shared_ptr<LfsFSImpl> _impl;
    fs::FS _fs;
};
