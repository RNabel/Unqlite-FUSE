/* This file was automatically generated.  Do not edit! */
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include "fs.h"

// data-types.
struct fcb {
    uuid_t path;
    uuid_t data;
    uuid_t name;
    off_t name_len; // Does not include null character.
    off_t path_len; // As above.
    off_t size;
    uuid_t uuid;

    // see 'man 2 stat' and 'man 2 chmod'
    //meta-data for the 'file'
    uid_t uid;
    /* user */
    gid_t gid;
    /* group */
    mode_t mode;
    /* protection */
    time_t mtime;
    /* time of last modification */
    time_t ctime;
    time_t atime;
    /* time of last change to meta-data (status) */
};

int newfs_getattr(const char *path, struct stat *stbuf);
int newfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int newfs_open(const char *path, struct fuse_file_info *fi);
int newfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int newfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int newfs_utime(const char *path, struct utimbuf *ubuf);
int newfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int newfs_chmod(const char *path, mode_t mode);

// --- paste auto generated below. ---
extern struct fcb rootDirectory;
int resolve_path(struct fcb *dir_fcb,char *path);
int resolve_path(struct fcb *dir_fcb,char *path);
int get_fcb(uuid_t *uuid,struct fcb *fetchedFCB);
int get_fcb(uuid_t *uuid,struct fcb *fetchedFCB);
int get_UUID_from_fcb(struct fcb *dir_fcb,off_t offset,uuid_t *uuid);
int get_UUID_from_fcb(struct fcb *dir_fcb,off_t offset,uuid_t *uuid);
int put_record(uuid_t *uuid,void *data,unqlite_int64 datasize);
int put_record(uuid_t *uuid,void *data,unqlite_int64 datasize);
void print_UUID(uuid_t *uuid);
int initialize_element(struct fcb *object,bool isDir);
void copy_stat_from_fcb(struct fcb *origin,struct stat *target);
bool is_dir(struct fcb *dir_fcb);
int set_name(struct fcb *dir,char *name);
int get_name(struct fcb *dir,char *name);
int set_path(struct fcb *dir,char *name);
int get_path(struct fcb *dir,char *path);
int set_data(struct fcb *dir,char *data,size_t size);
int get_data(struct fcb *dir,char *data);
int dat_truncate(struct fcb *dir,int new_size);
int dat_del_chunk(struct fcb *dir,int start_index,int size);
int dat_insert_chunk(struct fcb *dir,int start_index,char *insert_data,int size);
int get_record_size(uuid_t *uuid,void *data,unqlite_int64 size);
int delete_record(uuid_t *uuid);
int tokenize_path(char *path,char ***tokens2,int *count);
int separate_path(char *path,int* current);
int resolve_ancestor(struct fcb *ancestor_fcb,char *path,int ancestor_level);
int get_fcb_from_name(struct fcb *dir_fcb,char *name,struct fcb *found_el);
int remove_UUID_from_dir(struct fcb *dir_fcb,uuid_t *uuid);
int rm_element_from_directory(struct fcb *dir_fcb,char *path,bool delete_dir);
void store_thing();
int newfs_chmod(const char *path,mode_t mode);
int newfs_chown(const char *path,uid_t uid,gid_t gid);
int newfs_mkdir(const char *path_in,mode_t mode);
int newfs_unlink(const char *path);
int newfs_rmdir(const char *path_in);
int newfs_truncate(const char *path,off_t newsize);
int newfs_flush(const char *path,struct fuse_file_info *fi);
int newfs_release(const char *path,struct fuse_file_info *fi);
bool test_tokenization();
void run_test(bool test_res,char *error_string);
void test_endpoint();
void init_fs();
void shutdown_fs();
#if !defined(IS_LIB)
int main(int argc,char *argv[]);
#endif
#define INTERFACE 0
#define EXPORT_INTERFACE 0
#define LOCAL_INTERFACE 0
#define EXPORT
#define LOCAL static
#define PUBLIC
#define PRIVATE
#define PROTECTED
