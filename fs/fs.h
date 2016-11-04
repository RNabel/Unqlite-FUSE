#include <uuid/uuid.h>
#include <unqlite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <fuse.h>

extern unqlite_int64 root_object_size_value;
#define ROOT_OBJECT_KEY "root"
#define ROOT_OBJECT_KEY_SIZE 4
#define ROOT_OBJECT_SIZE_P (unqlite_int64 *)&root_object_size_value
#define ROOT_OBJECT_SIZE root_object_size_value
#define ROOT_OBJECT_ID root_object.id
#define ROOT_OBJECT_ID_P &(root_object.id)

#define KEY_SIZE 16

#define STUPID_MAX_PATH 100
#define STUPID_MAX_FILE_SIZE 100

#ifdef IS_LIB
#define DATABASE_NAME "/cs/scratch/rn30/test.db"
#else
#define DATABASE_NAME "newfs.db"
#endif

typedef struct rootS{
	uuid_t id;
}*root;

extern unqlite *pDb;
extern struct rootS root_object;
extern int root_is_empty;

extern void error_handler(int);
void print_id(uuid_t *);
void init_store();
int update_root();
int store_root();

extern FILE* init_log_file();
extern void write_log(const char *, ...);

struct newfs_state {
    FILE *logfile;
};
#define NEWFS_PRIVATE_DATA ((struct newfs_state *) fuse_get_context()->private_data)

// For use in example code only. Do not use this structure in your submission!
struct notafcb{
	char path[STUPID_MAX_PATH];
	char data[STUPID_MAX_FILE_SIZE];
	
	// see 'man 2 stat' and 'man 2 chmod'
	//meta-data for the 'file'
	uid_t  uid;		/* user */
    gid_t  gid;		/* group */
	mode_t mode;	/* protection */
	time_t mtime;	/* time of last modification */
	time_t ctime;	/* time of last change to meta-data (status) */
	off_t size;		/* size */
	
	//meta-data for the root thing (directory)
	uid_t  root_uid;		/* user */
    gid_t  root_gid;		/* group */
	mode_t root_mode;	/* protection */
	time_t root_mtime;	/* time of last modification */
};


