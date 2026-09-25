#include "FileSystemLFS.h"
#include "app/Log/Log.h"

#include <FSImpl.h>
#include <string.h>

// ---- geometry ---------------------------------------------------------
// read/prog granularity: the ESP32 flash API takes any size, and small
// units keep metadata commits compact, which postpones the 64 KB erase
// that compacting a directory costs
#define LFS_IO_SIZE 16
// per-filesystem read and program caches, plus one per open file
#define LFS_CACHE_SIZE 2048
// erases a metadata block takes before littlefs moves it for wear levelling
#define LFS_BLOCK_CYCLES 500

// ---- Arduino File / FS glue ---------------------------------------------

static bool parse_mode(const char *mode, int &flags)
{
    if (!mode || !*mode)
        return false;

    bool plus = strchr(mode, '+') != nullptr;
    switch (mode[0])
    {
    case 'r':
        flags = plus ? LFS_O_RDWR : LFS_O_RDONLY;
        return true;
    case 'w':
        flags = (plus ? LFS_O_RDWR : LFS_O_WRONLY) | LFS_O_CREAT | LFS_O_TRUNC;
        return true;
    case 'a':
        flags = (plus ? LFS_O_RDWR : LFS_O_WRONLY) | LFS_O_CREAT | LFS_O_APPEND;
        return true;
    default:
        return false;
    }
}

// create every directory above path, for open(..., create = true)
static void make_parents(lfs_t *lfs, const char *path)
{
    String p(path);
    for (int slash = p.indexOf('/', 1); slash > 0; slash = p.indexOf('/', slash + 1))
        lfs_mkdir(lfs, p.substring(0, slash).c_str());
}

class LfsFileImpl : public fs::FileImpl
{
public:
    LfsFileImpl(lfs_t *lfs, const char *path)
        : _lfs(lfs), _path(path), _isDir(false), _open(false)
    {
        int slash = _path.lastIndexOf('/');
        _name = slash < 0 ? _path : _path.substring(slash + 1);
    }

    ~LfsFileImpl() override
    {
        close();
    }

    bool openFile(int flags)
    {
        _isDir = false;
        _open = lfs_file_open(_lfs, &_file, _path.c_str(), flags) == LFS_ERR_OK;
        return _open;
    }

    bool openDir()
    {
        _isDir = true;
        _open = lfs_dir_open(_lfs, &_dir, _path.c_str()) == LFS_ERR_OK;
        return _open;
    }

    size_t write(const uint8_t *buf, size_t size) override
    {
        if (!_open || _isDir)
            return 0;
        lfs_ssize_t n = lfs_file_write(_lfs, &_file, buf, size);
        return n < 0 ? 0 : n;
    }

    size_t read(uint8_t *buf, size_t size) override
    {
        if (!_open || _isDir)
            return 0;
        lfs_ssize_t n = lfs_file_read(_lfs, &_file, buf, size);
        return n < 0 ? 0 : n;
    }

    void flush() override
    {
        if (_open && !_isDir)
            lfs_file_sync(_lfs, &_file);
    }

    bool seek(uint32_t pos, SeekMode mode) override
    {
        if (!_open || _isDir)
            return false;
        int whence = mode == SeekCur ? LFS_SEEK_CUR : mode == SeekEnd ? LFS_SEEK_END : LFS_SEEK_SET;
        return lfs_file_seek(_lfs, &_file, pos, whence) >= 0;
    }

    size_t position() const override
    {
        if (!_open || _isDir)
            return 0;
        lfs_soff_t pos = lfs_file_tell(_lfs, &_file);
        return pos < 0 ? 0 : pos;
    }

    size_t size() const override
    {
        if (!_open || _isDir)
            return 0;
        lfs_soff_t size = lfs_file_size(_lfs, &_file);
        return size < 0 ? 0 : size;
    }

    bool setBufferSize(size_t size) override
    {
        // littlefs buffers through its own per-file cache
        return false;
    }

    void close() override
    {
        if (!_open)
            return;
        if (_isDir)
            lfs_dir_close(_lfs, &_dir);
        else
            lfs_file_close(_lfs, &_file);
        _open = false;
    }

    time_t getLastWrite() override
    {
        return 0;
    }

    const char *path() const override
    {
        return _path.c_str();
    }

    const char *name() const override
    {
        return _name.c_str();
    }

    boolean isDirectory(void) override
    {
        return _isDir;
    }

    fs::FileImplPtr openNextFile(const char *mode) override;

    boolean seekDir(long position) override
    {
        return _open && _isDir && lfs_dir_seek(_lfs, &_dir, position) == LFS_ERR_OK;
    }

    String getNextFileName(void) override
    {
        return nextEntry(nullptr);
    }

    String getNextFileName(bool *isDir) override
    {
        return nextEntry(isDir);
    }

    void rewindDirectory(void) override
    {
        if (_open && _isDir)
            lfs_dir_rewind(_lfs, &_dir);
    }

    operator bool() override
    {
        return _open;
    }

private:
    // full path of the next entry, skipping . and .., or "" at the end
    String nextEntry(bool *isDir)
    {
        if (!_open || !_isDir)
            return "";

        struct lfs_info info;
        while (lfs_dir_read(_lfs, &_dir, &info) > 0)
        {
            if (!strcmp(info.name, ".") || !strcmp(info.name, ".."))
                continue;

            String entry = _path;
            if (!entry.endsWith("/"))
                entry += "/";
            entry += info.name;
            if (isDir)
                *isDir = info.type == LFS_TYPE_DIR;
            return entry;
        }
        return "";
    }

    lfs_t *_lfs;
    String _path;
    String _name;
    bool _isDir;
    bool _open;
    // lfs_file_tell and lfs_file_size take a non-const handle
    mutable lfs_file_t _file;
    lfs_dir_t _dir;
};

// Directories open for reading only, like the stock VFS filesystems.
static fs::FileImplPtr open_path(lfs_t *lfs, const char *path, const char *mode, bool create)
{
    int flags;
    if (!lfs || !path || path[0] != '/' || !parse_mode(mode, flags))
        return fs::FileImplPtr();

    auto file = std::make_shared<LfsFileImpl>(lfs, path);

    struct lfs_info info;
    if (lfs_stat(lfs, path, &info) == LFS_ERR_OK && info.type == LFS_TYPE_DIR)
    {
        if (flags != LFS_O_RDONLY || !file->openDir())
            return fs::FileImplPtr();
        return file;
    }

    if (create && (flags & LFS_O_CREAT))
        make_parents(lfs, path);

    if (!file->openFile(flags))
        return fs::FileImplPtr();
    return file;
}

fs::FileImplPtr LfsFileImpl::openNextFile(const char *mode)
{
    String entry = nextEntry(nullptr);
    if (entry.length() == 0)
        return fs::FileImplPtr();
    return open_path(_lfs, entry.c_str(), mode, false);
}

// Backs the fs::FS handed out by FileSystemLFS::fs(). lfs is null while
// the partition is unmounted, which makes every call fail cleanly.
class LfsFSImpl : public fs::FSImpl
{
public:
    lfs_t *lfs = nullptr;

    fs::FileImplPtr open(const char *path, const char *mode, const bool create) override
    {
        return open_path(lfs, path, mode, create);
    }

    bool exists(const char *path) override
    {
        struct lfs_info info;
        return lfs && lfs_stat(lfs, path, &info) == LFS_ERR_OK;
    }

    bool rename(const char *pathFrom, const char *pathTo) override
    {
        return lfs && lfs_rename(lfs, pathFrom, pathTo) == LFS_ERR_OK;
    }

    bool remove(const char *path) override
    {
        return lfs && lfs_remove(lfs, path) == LFS_ERR_OK;
    }

    bool mkdir(const char *path) override
    {
        return lfs && lfs_mkdir(lfs, path) == LFS_ERR_OK;
    }

    bool rmdir(const char *path) override
    {
        struct lfs_info info;
        return lfs && lfs_stat(lfs, path, &info) == LFS_ERR_OK &&
               info.type == LFS_TYPE_DIR && lfs_remove(lfs, path) == LFS_ERR_OK;
    }
};

// ---- FileSystemLFS ------------------------------------------------------

FileSystemLFS::FileSystemLFS(const char *partitionLabel, uint32_t blockSize, bool formatOnFail)
    : _partitionLabel(partitionLabel),
      _blockSize(blockSize),
      _formatOnFail(formatOnFail),
      _mounted(false),
      _partition(nullptr),
      _mutex(nullptr),
      _impl(std::make_shared<LfsFSImpl>()),
      _fs(_impl)
{
    memset(&_lfs, 0, sizeof(_lfs));
    memset(&_cfg, 0, sizeof(_cfg));
}

FileSystemLFS::~FileSystemLFS()
{
    end();
    if (_mutex)
        vSemaphoreDelete(_mutex);
}

// Finds the partition and fills in the littlefs geometry. Refuses a
// partition that does not sit on whole erase blocks, since an erase would
// then reach into its neighbour.
bool FileSystemLFS::configure()
{
    if (!_mutex)
        _mutex = xSemaphoreCreateMutex();
    if (!_mutex)
    {
        _log("LFS: cannot create mutex\n");
        return false;
    }

    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                          ESP_PARTITION_SUBTYPE_ANY,
                                          _partitionLabel);
    if (!_partition)
    {
        _log("LFS: no partition named %s\n", _partitionLabel);
        return false;
    }

    if (_blockSize == 0 || _blockSize % LFS_CACHE_SIZE != 0 ||
        _partition->address % _blockSize != 0 || _partition->size % _blockSize != 0)
    {
        _log("LFS: partition %s at 0x%x size 0x%x is not aligned to %u byte blocks\n",
             _partitionLabel,
             (unsigned)_partition->address,
             (unsigned)_partition->size,
             (unsigned)_blockSize);
        return false;
    }

    memset(&_cfg, 0, sizeof(_cfg));
    _cfg.context = this;
    _cfg.read = flashRead;
    _cfg.prog = flashProg;
    _cfg.erase = flashErase;
    _cfg.sync = flashSync;
    _cfg.lock = lock;
    _cfg.unlock = unlock;

    _cfg.read_size = LFS_IO_SIZE;
    _cfg.prog_size = LFS_IO_SIZE;
    _cfg.block_size = _blockSize;
    _cfg.block_count = _partition->size / _blockSize;
    _cfg.block_cycles = LFS_BLOCK_CYCLES;
    _cfg.cache_size = LFS_CACHE_SIZE;
    // one bit per block, rounded up to the 8 byte multiple littlefs needs,
    // so a single scan covers the whole partition
    _cfg.lookahead_size = ((_cfg.block_count + 63) / 64) * 8;

    return true;
}

bool FileSystemLFS::begin()
{
    if (_mounted)
        return true;

    _log("LFS Flash Init: partition=%s block=%u\n", _partitionLabel, (unsigned)_blockSize);

    if (!configure())
        return false;

    int err = lfs_mount(&_lfs, &_cfg);

    // CORRUPT: no littlefs superblock (blank, or still holding the old FAT
    // volume). INVAL: littlefs, but formatted with another geometry. I/O
    // errors are left alone rather than wiping a partition that may be fine.
    if ((err == LFS_ERR_CORRUPT || err == LFS_ERR_INVAL) && _formatOnFail)
    {
        _log("LFS: no usable filesystem (%d), formatting %s\n", err, _partitionLabel);
        err = lfs_format(&_lfs, &_cfg);
        if (err == LFS_ERR_OK)
            err = lfs_mount(&_lfs, &_cfg);
    }

    if (err != LFS_ERR_OK)
    {
        _log("LFS Flash Init Failed: %d\n", err);
        return false;
    }

    _mounted = true;
    _impl->lfs = &_lfs;

    _log("LFS Flash Init OK: total=%u used=%u\n",
         (unsigned)totalBytes(),
         (unsigned)usedBytes());

    return true;
}

void FileSystemLFS::end()
{
    if (!_mounted)
        return;

    _impl->lfs = nullptr;
    lfs_unmount(&_lfs);
    _mounted = false;

    _log("LFS Flash End\n");
}

bool FileSystemLFS::format()
{
    bool wasMounted = _mounted;
    end();

    _log("LFS Flash Format: partition=%s\n", _partitionLabel);

    if (!configure())
        return false;

    int err = lfs_format(&_lfs, &_cfg);
    if (err != LFS_ERR_OK)
    {
        _log("LFS Flash Format Failed: %d\n", err);
        return false;
    }

    _log("LFS Flash Format OK\n");

    return wasMounted ? begin() : true;
}

File FileSystemLFS::open(const char *path, const char *mode)
{
    if (!_mounted)
    {
        _log("LFS Flash open failed, not mounted: %s\n", path);
        return File();
    }

    return _fs.open(path, mode);
}

bool FileSystemLFS::exists(const char *path)
{
    return _mounted && _fs.exists(path);
}

bool FileSystemLFS::remove(const char *path)
{
    return _mounted && _fs.remove(path);
}

bool FileSystemLFS::rename(const char *pathFrom, const char *pathTo)
{
    return _mounted && _fs.rename(pathFrom, pathTo);
}

bool FileSystemLFS::mkdir(const char *path)
{
    return _mounted && (_fs.exists(path) || _fs.mkdir(path));
}

File FileSystemLFS::openDir(const char *path)
{
    if (!_mounted)
        return File();

    return _fs.open(path, FILE_READ);
}

size_t FileSystemLFS::totalBytes()
{
    if (!_mounted)
        return 0;

    return (size_t)_cfg.block_count * _cfg.block_size;
}

// littlefs counts whole blocks, so a small file still reports as a 64 KB
// block unless it is small enough to be stored inline in its directory
size_t FileSystemLFS::usedBytes()
{
    if (!_mounted)
        return 0;

    lfs_ssize_t blocks = lfs_fs_size(&_lfs);
    return blocks < 0 ? 0 : (size_t)blocks * _cfg.block_size;
}

size_t FileSystemLFS::freeBytes()
{
    size_t total = totalBytes();
    size_t used = usedBytes();
    return used < total ? total - used : 0;
}

// ---- block device -------------------------------------------------------
// Erases go through esp_partition_erase_range on whole, aligned blocks, which
// the flash driver issues as 64 KB block erases rather than 4 KB sector erases.

int FileSystemLFS::flashRead(const struct lfs_config *c, lfs_block_t block,
                             lfs_off_t off, void *buffer, lfs_size_t size)
{
    auto *self = static_cast<FileSystemLFS *>(c->context);
    esp_err_t err = esp_partition_read(self->_partition,
                                       (size_t)block * c->block_size + off,
                                       buffer, size);
    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int FileSystemLFS::flashProg(const struct lfs_config *c, lfs_block_t block,
                             lfs_off_t off, const void *buffer, lfs_size_t size)
{
    auto *self = static_cast<FileSystemLFS *>(c->context);
    esp_err_t err = esp_partition_write(self->_partition,
                                        (size_t)block * c->block_size + off,
                                        buffer, size);
    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int FileSystemLFS::flashErase(const struct lfs_config *c, lfs_block_t block)
{
    auto *self = static_cast<FileSystemLFS *>(c->context);
    esp_err_t err = esp_partition_erase_range(self->_partition,
                                              (size_t)block * c->block_size,
                                              c->block_size);
    if (err != ESP_OK)
    {
        _log("LFS: erase of block %u failed: %s\n", (unsigned)block, esp_err_to_name(err));
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

int FileSystemLFS::flashSync(const struct lfs_config *c)
{
    // writes go straight to flash; nothing is held back
    return LFS_ERR_OK;
}

int FileSystemLFS::lock(const struct lfs_config *c)
{
    auto *self = static_cast<FileSystemLFS *>(c->context);
    return xSemaphoreTake(self->_mutex, portMAX_DELAY) == pdTRUE ? LFS_ERR_OK : LFS_ERR_IO;
}

int FileSystemLFS::unlock(const struct lfs_config *c)
{
    auto *self = static_cast<FileSystemLFS *>(c->context);
    xSemaphoreGive(self->_mutex);
    return LFS_ERR_OK;
}
