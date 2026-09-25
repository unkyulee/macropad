// Build options and symbol renames for this project's copy of littlefs.
//
// The Arduino core already links a prebuilt littlefs (the esp_littlefs
// component) that exports the same lfs_* names, compiled with different
// options. Renaming ours keeps the linker from ever resolving one of our
// calls into that copy, whose structures may not match ours.
//
// Included from the top of lfs_util.h, so lfs.c, lfs_util.c and every file
// that includes lfs.h see the same options and names.
#ifndef LFS_PORT_H
#define LFS_PORT_H

// lfs_config gains lock/unlock hooks; the storage adapter backs them with a
// FreeRTOS mutex, because the web server and the GIF player touch files
// from different tasks.
#define LFS_THREADSAFE

// keep warnings and errors, drop the chatty debug trace
#define LFS_NO_DEBUG

#define lfs_crc              mp_lfs_crc
#define lfs_dir_close        mp_lfs_dir_close
#define lfs_dir_open         mp_lfs_dir_open
#define lfs_dir_read         mp_lfs_dir_read
#define lfs_dir_rewind       mp_lfs_dir_rewind
#define lfs_dir_seek         mp_lfs_dir_seek
#define lfs_dir_tell         mp_lfs_dir_tell
#define lfs_file_close       mp_lfs_file_close
#define lfs_file_open        mp_lfs_file_open
#define lfs_file_opencfg     mp_lfs_file_opencfg
#define lfs_file_read        mp_lfs_file_read
#define lfs_file_rewind      mp_lfs_file_rewind
#define lfs_file_seek        mp_lfs_file_seek
#define lfs_file_size        mp_lfs_file_size
#define lfs_file_sync        mp_lfs_file_sync
#define lfs_file_tell        mp_lfs_file_tell
#define lfs_file_truncate    mp_lfs_file_truncate
#define lfs_file_write       mp_lfs_file_write
#define lfs_format           mp_lfs_format
#define lfs_fs_gc            mp_lfs_fs_gc
#define lfs_fs_grow          mp_lfs_fs_grow
#define lfs_fs_mkconsistent  mp_lfs_fs_mkconsistent
#define lfs_fs_size          mp_lfs_fs_size
#define lfs_fs_stat          mp_lfs_fs_stat
#define lfs_fs_traverse      mp_lfs_fs_traverse
#define lfs_getattr          mp_lfs_getattr
#define lfs_migrate          mp_lfs_migrate
#define lfs_mkdir            mp_lfs_mkdir
#define lfs_mount            mp_lfs_mount
#define lfs_remove           mp_lfs_remove
#define lfs_removeattr       mp_lfs_removeattr
#define lfs_rename           mp_lfs_rename
#define lfs_setattr          mp_lfs_setattr
#define lfs_stat             mp_lfs_stat
#define lfs_unmount          mp_lfs_unmount

#endif
