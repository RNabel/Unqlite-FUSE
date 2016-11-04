/*
  NewFS. One directory, one file, 100 bytes of storage. What more do you need?
  
  This Fuse file system is based largely on the HelloWorld example by Miklos Szeredi <miklos@szeredi.hu> (http://fuse.sourceforge.net/helloworld.html). Additional inspiration was taken from Joseph J. Pfeiffer's "Writing a FUSE Filesystem: a Tutorial" (http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/).
*/

#define FUSE_USE_VERSION 26

#define STUPID_MAX_NAME 30

#ifdef IS_LIB
#define LOCAL
#else
#define LOCAL static
#endif

#include <time.h>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

#include "fs.h"

struct fcb {
    uuid_t path;
    uuid_t data;
    uuid_t name;
    off_t name_len; //  Does not include null character.
    off_t path_len; // As above.
    off_t size;
    /* size */
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
    /* time of last change to meta-data (status) */
    time_t atime;     /* time of last change to meta-data (status) */
} rootDirectory;

// --- Necessary predeclarations. ---
int resolve_path(struct fcb *dir_fcb, char *path);

int get_fcb(uuid_t *uuid, struct fcb *fetchedFCB);

int get_UUID_from_fcb(struct fcb *dir_fcb, off_t offset, uuid_t *uuid);

int put_record(uuid_t *uuid, void *data, unqlite_int64 datasize);

int get_record_size(uuid_t *uuid, void *data, unqlite_int64 size);
int get_fcb_from_name(struct fcb *dir_fcb, char *name, struct fcb *found_el);

// ---- FCB related. ----
// Print UUID.
void print_UUID(uuid_t *uuid) {
    int i;
    for (i = 0; i < sizeof *uuid; i++) {
        printf("%02x ", *uuid[i]);
    }
}

// Creates directory or file FCB.
int initialize_element(struct fcb *object, bool isDir) {
    object->size = 0;
    object->path_len = 0;
    object->name_len = 0;
    time(&(object->mtime));
    time(&(object->atime));
    time(&(object->ctime));

    if (isDir) { // If object is directory.
        object->mode = S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;

    } else { // If object is file.
        object->mode = S_IFREG;
    }

    object->mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    object->uid = getuid();
    object->gid = getgid();

    // Generate a uuid.
    uuid_generate_random(object->uuid);
    // Generate uuid for data field.
    uuid_generate_random(object->data);
    put_record(&object->data, 0, 0);

    // Generate uuid for name and path.
    uuid_generate_random(object->path);
    put_record(&object->path, '\0', 1);
    uuid_generate_random(object->name);
    put_record(&object->name, '\0', 1);
    return 0;
}

void copy_stat_from_fcb(struct fcb *origin, struct stat *target) {
    // Reset the target struct.
    memset(target, 0, sizeof(struct stat));

    target->st_dev = 0;     /* ID of device containing file */
    target->st_ino = 0;     /* inode number */
    target->st_mode = origin->mode;    /* protection */
    target->st_nlink = 1;   /* number of hard links */
    target->st_uid = origin->uid;     /* user ID of owner */
    target->st_gid = origin->gid;     /* group ID of owner */
    target->st_rdev = 0;    /* device ID (if special file) */
    target->st_size = origin->size;    /* total size, in bytes */
    target->st_blksize = 0; /* blocksize for file system I/O */
    target->st_blocks = 0;  /* number of 512B blocks allocated */
    target->st_atime = origin->atime;   /* time of last access */
    target->st_mtime = origin->mtime;   /* time of last modification */
    target->st_ctime = origin->ctime;   /* time of last status change */
}

bool is_dir(struct fcb *dir_fcb) {
    return S_ISDIR(dir_fcb->mode);
}

// Gets an FCB struct from UUID.
int get_fcb(uuid_t *uuid, struct fcb *fetchedFCB) {
    unqlite_int64 nBytes;  //Data length.
    int rc = unqlite_kv_fetch(pDb, uuid, KEY_SIZE, NULL, &nBytes);
    if (rc != UNQLITE_OK) {
        error_handler(rc);
    }
    if (nBytes != sizeof(struct fcb)) {
        printf("Data object has unexpected size. Doing nothing.(get_fcb)\n");
        exit(-1);
    }

    // Fetch data.
    unqlite_kv_fetch(pDb, uuid, KEY_SIZE, fetchedFCB, &nBytes);

    return 0;
}

// FCB getters and setters.
int set_name(struct fcb *dir, char *name) {
    int rc = put_record(&dir->name, name, (unqlite_int64) (strlen(name) + 1));
    dir->name_len = strlen(name);
    return rc;
}

int get_name(struct fcb *dir, char *name) {
    int rc = get_record_size(&dir->name, name, dir->name_len + 1);
    return rc;
}

int set_path(struct fcb *dir, char *name) {
    int rc = put_record(&dir->path, name, (unqlite_int64) (strlen(name) + 1));
    dir->path_len = strlen(name);
    return rc;
}

int get_path(struct fcb *dir, char *path) {
    int rc = get_record_size(&dir->path, path, dir->path_len + 1);
    return rc;
}

int set_data(struct fcb *dir, char *data, size_t size) {
    int rc = put_record(&dir->data, data, (unqlite_int64) size);
    dir->size = size;
    return rc;
}

int get_data(struct fcb *dir, char *data) {
    int rc = get_record_size(&dir->data, data, dir->size);
    return rc;
}

// ---- FCB data helpers. ----
// Function which manipulate the data of a file object, and then flushes the changes to the database.
// All return 1 on error.

int dat_truncate(struct fcb *dir, int new_size) {
    int curr_size = (int) dir->size;
    if (new_size <= curr_size && new_size >= 0) {
        char data[curr_size];
        get_data(dir, data);

        // Update the size.
        dir->size = new_size;
        set_data(dir, data, (size_t) new_size);

        // Save FCB to backing store.
        put_record(&dir->uuid, dir, sizeof(struct fcb));
        return 0;
    } else {
        // Error.
        return 1;
    }
}

int dat_del_chunk(struct fcb *dir, int start_index, int size) {
    // Input validation.
    int end_index = start_index + size - 1;
    int dat_size = (int) dir->size;
    if (end_index < dat_size && start_index >= 0) {
        // Get data.
        char data[dat_size];
        get_data(dir, data);

        // Shift other content.
        int next_index = end_index + 1;
        int bytes_to_copy = dat_size - next_index;
        memmove(&data[start_index], &data[next_index], (size_t) bytes_to_copy);

        // Set updated data.
        int new_total_size = dat_size - size;
        dir->size = new_total_size;
        set_data(dir, data, (size_t) new_total_size);

        // Update FCB in backing store.
        put_record(&dir->uuid, dir, sizeof(struct fcb));
        return 0;
    } else {
        // Return error.
        return 1;
    }
}

// If -1 index is passed, data is appended.
int dat_insert_chunk(struct fcb *dir, int start_index, char *insert_data, int size) {
    int curr_size = (int) dir->size;
    if (start_index == -1) {
        start_index = curr_size;
    }

    if (start_index > curr_size || start_index < 0) { // Error occurred
        return 1;
    } else {
        // Get the data.
        int new_data_size = curr_size + size;
        char dir_data[new_data_size];
        get_data(dir, dir_data);

        // Shift content from start_index to start_index + size
        int bytes_to_copy = curr_size - start_index;
        memmove(&dir_data[start_index + size], &dir_data[start_index], (size_t) bytes_to_copy);

        memcpy(&dir_data[start_index], insert_data, (size_t) size);

        // Set data.
        set_data(dir, dir_data, (size_t) new_data_size);

        // Save changes of fcb.
        put_record(&dir->uuid, dir, sizeof(struct fcb));
        return 0;
    }
}

int dat_get_chunk(struct fcb *file, int start_index, int size, char *buffer) {
    if (size < 0 || start_index < 0 || buffer == NULL) {
        return 0;
    }

    // Calculate the maximum size which can be extracted.
    off_t bytes_copied = file->size - start_index;
    bytes_copied = (bytes_copied <= size) ? bytes_copied : size;

    char data[file->size];
    get_data(file, data);
    memcpy(buffer, &data[start_index], bytes_copied);

    return bytes_copied;
}

// ---- Database access shorthands. ----
int get_record_size(uuid_t *uuid, void *data, unqlite_int64 size) {
    unqlite_int64 nBytes;
    int rc = unqlite_kv_fetch(pDb, uuid, KEY_SIZE, NULL, &nBytes);
    if (rc != UNQLITE_OK) {
        error_handler(rc);
    }
    if (size != 0 && nBytes != size) {
        printf("Data object has unexpected size. Doing nothing.(get_record)\n");
        exit(-1);
    }

    // Fetch data.
    unqlite_kv_fetch(pDb, uuid, KEY_SIZE, data, &nBytes);

    return 0;
}

int put_record(uuid_t *uuid, void *data, unqlite_int64 datasize) {

    // Store data object.
    int rc = unqlite_kv_store(pDb, uuid, KEY_SIZE, data, datasize);

    if (rc != UNQLITE_OK) {
        error_handler(rc);
    }
    return 0;
}

int delete_record(uuid_t *uuid) {
    int rc = unqlite_kv_delete(pDb, uuid, KEY_SIZE);
    return rc;
}

// ---- Path related functionality. ----

// Separates a string by the '/' character.
// tokens parameter is set to point the array of tokens.
// IMPORTANT: Requires freeing after assignment.
int tokenize_path(char *path, char ***tokens2, int *count) {
    size_t path_len = strlen(path);
    // Input validation.
    if (path_len == 0) return 0;

    int i = 0;
    int numOfSlashes = 0;

    // Check whether there are leading or trailing slashes.
    bool has_leading_slash = (path[0] == '/');
    bool has_trailing_slash = (path[path_len - 1] == '/');

    // Copy path.
    char pathCopy[strlen(path) + 1];
    strcpy(pathCopy, path);
    // Detect number of slashes.
    for (i = 0; i < strlen(path); ++i) {
        if (path[i] == '/') numOfSlashes++;
    }

    *count = numOfSlashes + 1;
    char **tokens = malloc((numOfSlashes + 1) * sizeof(char *));
    *tokens2 = tokens;

    // Split string by "/".
    char *token = strtok(pathCopy, "/");
    int current_index = 0;
    int num_tokens_to_read = *count;

    // Adjust for special case of leading and trailing slash.
    if (has_leading_slash) {
        current_index++;
        num_tokens_to_read--;
    }
    if (has_trailing_slash) {
        num_tokens_to_read--;
    }

    while (token != NULL) {
        size_t token_len = strlen(token);

        // Set pointer in array.
        tokens[current_index] = (char *) malloc((token_len + 1) * sizeof(char));

        // Copy memory over.
        memcpy(tokens[current_index], token, token_len + 1);

        // Get next token.
        token = strtok(NULL, "/");

        num_tokens_to_read++;
        current_index++;
    }

    if (has_leading_slash) {
        tokens[0] = (char *) malloc(sizeof(char));
        tokens[0][0] = '\0';
    }
    if (has_trailing_slash) {
        tokens[*count - 1] = (char *) malloc(sizeof(char));
        tokens[*count - 1][0] = '\0';
    }

    // Check for error.
    if (current_index + (has_trailing_slash) == *count) {
        return 0;
    } else {
        // Error occurred.
        return ENOENT;
    }
}

// Resolves a path and places FCB in id_pointer.
int resolve_path(struct fcb *dir_fcb, char *path) {

    // TODO needs to change.
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) { // If root was requested.
        get_record_size(&root_object.id, dir_fcb, sizeof(struct fcb));
        return 0;
    }
    // Get root object from database.
    struct fcb root;
    resolve_path(&root, "/");

    // Split the path into segments.
    char **tokens;
    int num_of_elements;
    int rc = tokenize_path(path, &tokens, &num_of_elements);

    // Traverse through the path.
    bool path_error = 1;
    int i = (tokens[0][0] == '\0') ? 1 : 0; // Check if it is a relative path.
    struct fcb last_fcb = root;
    struct fcb current_fcb;
    for (; i < num_of_elements; ++i) { // For each element in the path.
        char *current_token = tokens[i];

        // Get fcb corresponding to the name.
        rc = get_fcb_from_name(&last_fcb, current_token, &current_fcb);
        if (rc != 0) { // Break if the directory could not be found.
            break;
        } else {
            // Copy over the fcb
            memcpy(&last_fcb, &current_fcb, sizeof(struct fcb));
            if ((i != num_of_elements - 1) && !is_dir(&last_fcb)) { // if ancestor is non-folder.
                rc = ENOTDIR;
                break;
            }
        }

        // Free each element of the token array. // TODO move before the if statement to avoid mem leak.
        free(tokens[i]);
    } // For each element in path.

    // Free the token array indices.
    free(tokens);

    if (rc != 0) {
        return rc;
    } else {
        memcpy(dir_fcb, &last_fcb, sizeof(struct fcb));
        return 0;
    }

}

// Separates the path passed in with a null character.
int separate_path(char *path, int *current) {
    int path_len = (int) strlen(path);
    if (path_len < 1) {
        *current = 0;
        return 0;
    }

    // Get address of parent dir.
    int i;

    // Delete trailing slash.
    if (path[path_len - 1] == '/') {
        path[path_len - 1] = '\0';
    }
    for (i = path_len - 2; i >= 0; i--) { // Find last slash, ignoring trailing slash.
        if (path[i] == '/') {
            break;
        }
    }
    *current = i + 1;
    path[i] = '\0';
    return 0;
}

// Find the FCB of the ancestor at level specified by ancestor_level, where ancestor_level 0 is self,
//     and ancestor, level 1 is parent etc.
// Returns EFAULT if ancestor level negative, and sets ancestor_fcb to the root element if ancestor level is too great.
int resolve_ancestor(struct fcb *ancestor_fcb, char *path, int ancestor_level) {
    size_t path_len = strlen(path);
    if (ancestor_level < 0) {
        return EFAULT;
    }
    // Get address of parent dir.
    int i;

    // Delete trailing slash.
    if (path[path_len - 1] == '/') {
        path[path_len - 1] = '\0';
    }

    bool anc_found = !ancestor_level;
    for (i = path_len - 2; i >= 0 && ancestor_level != 0; i--) { // Find last slash, ignoring trailing slash.
        if (path[i] == '/') {
            ancestor_level--;
            if (ancestor_level == 0) {
                path[i] = '\0'; // Create separator.
                anc_found = true;
                break;
            }
        }
    }

    // Return root element if element is not found.
    if (!anc_found) {
        get_fcb(&root_object.id, ancestor_fcb);
        return 0;
    }

    return resolve_path(ancestor_fcb, path);
}

// --- Directory related code. ---

// READDIR - Gets the UUID at offset.
// Returns 1 if successful, and 0 if failed.
int get_UUID_from_fcb(struct fcb *dir_fcb, off_t offset, uuid_t *uuid) {
    // TODO check data field, possibly check.
    char data_ptr[dir_fcb->size];
    get_data(dir_fcb, data_ptr);

    offset = KEY_SIZE * offset;
    // If the offset can be retrieved.
    if (dir_fcb->size >= offset + KEY_SIZE) {
        memcpy(uuid, &data_ptr[offset], KEY_SIZE);
        return 1;

    } else {
        return 0;
    }
}

// Function which goes through all entries in the directory's data field, and finds the element named with the passed name.
int get_fcb_from_name(struct fcb *dir_fcb, char *name, struct fcb *found_el) {
    // Get the data block of the fcb which contains all UUIDs. TODO change to indirect reference.
    char data_block[dir_fcb->size];
    get_data(dir_fcb, data_block);

    off_t data_size = dir_fcb->size;
    off_t num_of_entries = data_size / KEY_SIZE;
    int current_index = 0;
    int return_code = ENOENT;

    // For each UUID in data buffer.
    for (current_index = 0; current_index < num_of_entries; current_index++) {
        uuid_t tmp_uuid;
        memcpy(&tmp_uuid, &data_block[current_index * KEY_SIZE], KEY_SIZE);

        // Get fcb from uuid.
        struct fcb tmp_fcb;
        get_record_size(&tmp_uuid, &tmp_fcb, sizeof(struct fcb));

        // Get name from FCB.
        char temp_name[tmp_fcb.name_len];
        get_name(&tmp_fcb, temp_name);

        if (strcmp(temp_name, name) == 0) { // If name corresponds to search term.
            memcpy(found_el, &tmp_fcb, sizeof(struct fcb));
            return_code = 0;
            break;
        }
    } // For each UUID in data buffer.

    return return_code;
}

// Returns 1 if uuid is not found.
int remove_UUID_from_dir(struct fcb *dir_fcb, uuid_t *uuid) {
    char data[dir_fcb->size];
    get_data(dir_fcb, data);

    off_t num_of_elements = dir_fcb->size / KEY_SIZE;
    off_t i;
    bool found = false;
    for (i = 0; i < num_of_elements; ++i) {
        // Get UUID at offset.
        uuid_t curr_uuid;
        get_UUID_from_fcb(dir_fcb, i, &curr_uuid);

        // Compare the UUID with the one looked for.
        int cmp = memcmp(&curr_uuid, uuid, sizeof(uuid_t));

        if (cmp == 0) { // If UUID is found, remove it.
            dat_del_chunk(dir_fcb, i * KEY_SIZE, KEY_SIZE);
            found = true;
            break;
        }
    }

    return (found) ? 0 : 1;
}

int rm_element_from_directory(struct fcb *dir_fcb, char *path, bool delete_dir) {

    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);

    if (is_dir(dir_fcb) == delete_dir) {
        // Get parent fcb.
        struct fcb parent_fcb;
        resolve_ancestor(&parent_fcb, path_copy, 1);

        // Delete UUID from parent_dir.
        remove_UUID_from_dir(&parent_fcb, &dir_fcb->uuid);

        // Delete UUID from backing store.
        delete_record(&dir_fcb->uuid);

        // Store updated parent FCB.
        put_record(&parent_fcb.uuid, &parent_fcb, sizeof(struct fcb));

    } else {
        // Handle errors for directory and file separately.
        if (delete_dir) {
            return ENOTDIR;
        } else {
            return EISDIR;
        }
    }
    return 0;
}

//Write the thing to disk. You will need something like this function in your implementation to write in-memory objects to the store.
void store_thing() {
    int datasize = sizeof(struct fcb);
    // Store data object.
    int rc = unqlite_kv_store(pDb, &(root_object.id), KEY_SIZE, &rootDirectory, datasize);
    if (rc != UNQLITE_OK) {
        error_handler(rc);
    }
}

//Get file and directory attributes (meta-data).
//Read 'man 2 stat' and 'man 2 chmod'.
LOCAL int newfs_getattr(const char *path, struct stat *stbuf) {
    int res = 0;

    write_log("newfs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuf);

    struct fcb object;
    int rc = resolve_path(&object, (char *) path);

    if (rc != ENOENT)
        copy_stat_from_fcb(&object, stbuf);
    else
        res = -ENOENT;

    return res;
}

//Read a directory.
//Read 'man 2 readdir'.
LOCAL int newfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    // Add current and parent folder.
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    // Get fcb of directory from path.
    struct fcb directory;
    int rc = resolve_path(&directory, (char *) path);
    if (rc != 0) {
        return -rc;
    }

    //Go through all entries in the FCB's data block and print them. ---
    // Get data block from UUID.
    off_t numOfDirs = directory.size / KEY_SIZE;

    // Loop through all entries in the directory's data block and print the name.
    int i;
    for (i = 0; i < numOfDirs; i++) {
        uuid_t current_el_uuid;
        rc = get_UUID_from_fcb(&directory, i, &current_el_uuid);

        struct fcb current_el_fcb;
        get_fcb(&current_el_uuid, &current_el_fcb);

        // Add the name to the buffer.
        // Drop leading '/' TODO get only last part of path.
        char element_name[current_el_fcb.name_len];
        get_name(&current_el_fcb, element_name);
        if (*element_name != '\0') {
            struct stat element_stat;
            copy_stat_from_fcb(&current_el_fcb, &element_stat);
            filler(buf, element_name, &element_stat, 0);
        }
    }

    write_log("write_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler,
              offset, fi);

    return 0;
}

//Open a file.
//Read 'man 2 open'.
LOCAL int newfs_open(const char *path_in, struct fuse_file_info *fi) {
    // Create copy of the path.
    char path[strlen(path_in) + 1];
    strcpy(path, path_in);

    write_log("newfs_open(path\"%s\", fi=0x%08x)\n", path, fi);

    // Check if file exists.
    struct fcb file_fcb;
    int rc = resolve_path(&file_fcb, path);
    if (rc != 0) {
        return -rc;
    }

    // Check if it is a file.
    if (is_dir(&file_fcb)) {
        return -EISDIR;
    }

    return 0;
}

//Read a file.
//Read 'man 2 read'.
LOCAL int newfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    write_log("newfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    // Check file exists.
    struct fcb file_fcb;
    int rc = resolve_path(&file_fcb, (char *) path);
    if (rc != 0) {
        return -rc;
    }

    // Check if file.
    if (is_dir(&file_fcb)) {
        return -EISDIR;
    }

    // Update access time.
    time(&file_fcb.atime);
    put_record(&file_fcb.uuid, &file_fcb, sizeof(struct fcb));

    rc = dat_get_chunk(&file_fcb, (int) offset, (int) size, buf);

    return rc;
}

//Read 'man 2 creat'.
LOCAL int newfs_create(const char *path_in, mode_t mode, struct fuse_file_info *fi) {
    // Create copy of the path.
    char path[strlen(path_in) + 1];
    strcpy(path, path_in);

    write_log("newfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

    // Check if already exists.
    struct fcb temp_fcb;
    int rc = resolve_path(&temp_fcb, path);
    if (rc != ENOENT) {
        if (rc == 0) {
            return -EEXIST;
        } else {
            return -rc;
        }
    }

    // Get parent directory.
    struct fcb parent_dir;
    char path_copy1[strlen(path) + 1];
    strcpy(path_copy1, path); // Create copy as resolve ancestor manipulates the array.
    rc = resolve_ancestor(&parent_dir, path_copy1, 1);
    if (rc != 0) {
        return -ENOENT;
    }

    // Initialize the FCB.
    struct fcb new_file;
    initialize_element(&new_file, false);
    if (mode != 0) {
        new_file.mode |= mode; // Copy over passed permissions.
    }

    // Get the file name.
    char path_copy2[strlen(path) + 1];
    strcpy(path_copy2, path); // Create copy
    int file_name_index;
    separate_path(path_copy2, &file_name_index);
    // Set path and directory name.
    set_name(&new_file, &path_copy2[file_name_index]);
    set_path(&new_file, (char *) path);

    // Append UUID of new directory to parent directory.
    dat_insert_chunk(&parent_dir, -1, new_file.uuid, KEY_SIZE);

    // Store old and new fcb in backing store.
    put_record(&parent_dir.uuid, &parent_dir, sizeof(struct fcb));
    put_record(&new_file.uuid, &new_file, sizeof(struct fcb));

    return 0;
}

//Set update the times (actime, modtime) for a file. This FS only supports modtime.
//Read 'man 2 utime'.
LOCAL int newfs_utime(const char *path, struct utimbuf *ubuf) {
    int retstat = 0;

    write_log("newfs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);

    struct fcb curr_dir;
    int rc = resolve_path(&curr_dir, (char *) path);
    if (rc != 0) {
        return -rc;
    }

    curr_dir.mtime = ubuf->modtime;
    curr_dir.atime = ubuf->actime;

    put_record(&curr_dir.uuid, &curr_dir, sizeof(struct fcb));

    return retstat;
}

//Write to a file.
//Read 'man 2 write'
LOCAL int newfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    write_log("newfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

    // Check file exists.
    struct fcb file_fcb;
    int rc = resolve_path(&file_fcb, (char *) path);
    if (rc != 0) {
        return -rc;
    }

    // Check if file.
    if (is_dir(&file_fcb)) {
        return -EISDIR;
    }

    // Update change time.
    time(&file_fcb.mtime);
    put_record(&file_fcb.uuid, &file_fcb, sizeof(struct fcb));

    rc = dat_insert_chunk(&file_fcb, (int) offset, (char *) buf, (int) size);
    if (rc != 0) {
        return -EXDEV;
    }

    // Save the FCB.
    put_record(&file_fcb.uuid, &file_fcb, sizeof(struct fcb));

    return size;
}

//Set permissions.
//Read 'man 2 chmod'.
LOCAL int newfs_chmod(const char *path, mode_t mode) {
    write_log("newfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);

    struct fcb curr_fcb;
    int rc = resolve_path(&curr_fcb, (char *) path);
    if (rc != 0) {
        return -rc;
    }

    // Set mode.
    curr_fcb.mode = mode;
    // Update time
    time(&curr_fcb.ctime);


    // Update stored FCB.
    put_record(&curr_fcb.uuid, &curr_fcb, sizeof(struct fcb));

    return 0;
}

//Set ownership.
//Read 'man 2 chown'.
int newfs_chown(const char *path, uid_t uid, gid_t gid) {
    struct fcb curr_fcb;
    int rc = resolve_path(&curr_fcb, (char *) path);
    if (rc != 0) {
        return -rc;
    }

    bool changed = false;
    if (uid != -1) {
        curr_fcb.uid = uid;
        changed = true;
    }
    if (gid != -1) {
        curr_fcb.gid = gid;
        changed = true;
    }

    // Update change time as adequate.
    if (changed) {
        time(&curr_fcb.ctime);
    }

    // Save the updated FCB.
    put_record(&curr_fcb.uuid, &curr_fcb, sizeof(struct fcb));

    write_log("newfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

    return 0;
}

//Create a directory.
//Read 'man 2 mkdir'.
int newfs_mkdir(const char *path_in, mode_t mode) {
    char path[strlen(path_in) + 1];
    strcpy(path, path_in);
    int retstat = 0;

    // Check if already exists.
    struct fcb tmp_dir;
    int rc = resolve_path(&tmp_dir, path);
    if (rc == 0) {
        return -EEXIST;
    }

    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);
    char path_copy1[strlen(path) + 1];
    strcpy(path_copy1, path);

    int dir_name_index;
    separate_path(path_copy, &dir_name_index);

    struct fcb parent_dir;
    rc = resolve_ancestor(&parent_dir, (char *) path_copy1, 1);
    if (rc == ENOENT) {
        return -ENOENT;
    }

    // Initialise new directory.
    struct fcb new_dir;
    initialize_element(&new_dir, true);

    // Take mode into account.
    if (mode != 0) {
        new_dir.mode |= mode;
    }

    // Set path and directory name.
    set_name(&new_dir, &path_copy[dir_name_index]);
    set_path(&new_dir, (char *) path);

    // Append UUID of new directory to parent directory.
    dat_insert_chunk(&parent_dir, -1, new_dir.uuid, KEY_SIZE);

    // Store old and new fcb in backing store.
    put_record(&parent_dir.uuid, &parent_dir, sizeof(struct fcb));
    put_record(&new_dir.uuid, &new_dir, sizeof(struct fcb));

    write_log("newfs_mkdir: %s\n", path);

    return retstat;
}

//Delete a file.
//Read 'man 2 unlink'.
int newfs_unlink(const char *path_in) {
    // Create copy of the path.
    char path[strlen(path_in) + 1];
    strcpy(path, path_in);

    // Get file's fcb.
    struct fcb file_fcb;
    int rc = resolve_path(&file_fcb, path);
    // Return error if does not exist.
    if (rc == ENOENT) {
        return -ENOENT;
    }

    rc = rm_element_from_directory(&file_fcb, path, false);

    write_log("newfs_unlink: %s\n", path);

    return -rc;
}

//Delete a directory.
//Read 'man 2 rmdir'.
int newfs_rmdir(const char *path_in) {
    // Create copy of the path.
    char path[strlen(path_in) + 1];
    strcpy(path, path_in);

    if (strcmp(path, "/") == 0) {
        // Return EBUSY as specified in man 2 rmdir.
        return -EBUSY;
    }

    // Get directory's fcb.
    struct fcb dir_fcb;
    int rc = resolve_path(&dir_fcb, path);
    // Return error if does not exist.
    if (rc == ENOENT) {
        return -ENOENT;
    }

    // Check that it is a directory.
    if (!is_dir(&dir_fcb)) {
        return -ENOTDIR;
    }

    // Check that directory is empty.
    if (dir_fcb.size != 0) {
        return -ENOTEMPTY;
    }

    rc = rm_element_from_directory(&dir_fcb, path, true);

    return -rc;
}

//Set the size of a file.
//Read 'man 2 truncate'.
int newfs_truncate(const char *path_in, off_t newsize) {
    if (newsize < 0) { // If size is negative, return error.
        return -EINVAL;
    }

    // Create copy of the path.
    char path[strlen(path_in) + 1];
    strcpy(path, path_in);
    write_log("newfs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);

    // Get the file.
    struct fcb curr_fcb;
    int rc = resolve_path(&curr_fcb, path);
    if (rc != 0) { // If file does not exist.
        return -rc;
    }

    // If it is a directory.
    if (is_dir(&curr_fcb)) {
        return -EISDIR;
    }

    if (newsize <= curr_fcb.size) { // If smaller use dat_truncate
        dat_truncate(&curr_fcb, (int) newsize);

    } else {
        // Get data and pad with zeroes.
        char data[newsize];
        memset(data, 0, newsize);
        get_data(&curr_fcb, data);
        set_data(&curr_fcb, data, newsize);
    }

    // Store updated fcb.
    put_record(&curr_fcb.uuid, &curr_fcb, sizeof(struct fcb));

    return 0;
}

//Flush any cached data. You don't need to do anything here unless you have implemented a caching mechanism.
int newfs_flush(const char *path, struct fuse_file_info *fi) {
    int retstat = 0;

    write_log("newfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);

    return retstat;
}

//Release the file. There will be one call to release for each call to open. You don't need to do anything here unless you have implemented a caching mechanism.
int newfs_release(const char *path, struct fuse_file_info *fi) {
    int retstat = 0;

    write_log("newfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

    return retstat;
}

LOCAL int newfs_rename(const char *path, const char *to) {
    struct fcb curr_el;
    int rc = resolve_path(&curr_el, (char *) path);
    if (rc != 0) {
        return -rc;
    }

    set_path(&curr_el, (char *) to);

    // Extract name.
    char to_copy[strlen(to) + 1];
    strcpy(to_copy, to);
    int name_start;
    separate_path(to_copy, &name_start);
    set_name(&curr_el, &to_copy[name_start]);

    // Save the FCB.
    put_record(&curr_el.uuid, &curr_el, sizeof(struct fcb));

    return 0;
}


static struct fuse_operations newfs_oper = {
        .getattr    = newfs_getattr,
        .readdir    = newfs_readdir,
        .open        = newfs_open,
        .read        = newfs_read,
        .create        = newfs_create,
        .utime        = newfs_utime,
        .write        = newfs_write,
        .truncate    = newfs_truncate,
        .flush        = newfs_flush,
        .release    = newfs_release,
        .mkdir      = newfs_mkdir,
        .rename     = newfs_rename,
        .chmod      = newfs_chmod,
        .chown      = newfs_chown,
        .unlink     = newfs_unlink,
        .rmdir      = newfs_rmdir
};

// --- TESTS. ---
bool test_tokenization() {
    // TESTING ONLY.
    char **tokens;
    int count;
    int rc = 0;
    rc = tokenize_path("asd", &tokens, &count);
    if (rc == ENOENT || count != 1 // TODO for testing only.
        || strcmp(tokens[0], "asd") != 0)
        return false;


    rc = tokenize_path("asd//last", &tokens, &count);
    if (rc != ENOENT)
        return false;

    rc = tokenize_path("/asd/last/", &tokens, &count);
    if (rc == ENOENT ||
        strcmp(tokens[0], "") != 0 ||
        strcmp(tokens[1], "asd") != 0 ||
        strcmp(tokens[2], "last") != 0 ||
        strcmp(tokens[3], "") != 0 ||
        count != 4)
        return false;

    rc = tokenize_path("/", &tokens, &count);
    if (rc == ENOENT || count != 2 ||
        strcmp(tokens[0], "") != 0 ||
        strcmp(tokens[1], "") != 0)
        return false;


    return true;
}

void run_test(bool test_res, char *error_string) {
    if (!test_res) {
        perror(error_string);
    }
}

void test_endpoint() {
    run_test(test_tokenization(), "Tokenization tests failed.");
}


//Initialise the in-memory data structures from the store. If the0 root object (from the store) is empty then create a root thing (directory) and write it to the store.
void init_fs() {
    int rc;
    write_log_direct("init_fs\n");
    //Initialise the store.
    init_store();
    if (!root_is_empty) {
        write_log_direct("init_fs: root is not empty\n");

        uuid_t *dataid = &(root_object.id);

        unqlite_int64 nBytes;  //Data length.
        rc = unqlite_kv_fetch(pDb, dataid, KEY_SIZE, NULL, &nBytes);
        if (rc != UNQLITE_OK) {
            error_handler(rc);
        }
        if (nBytes != sizeof(struct fcb)) {
            printf("Data object has unexpected size. Doing nothing.(init_fs)\n");
            exit(-1);
        }

        //Fetch the directory that the root object points at.
        unqlite_kv_fetch(pDb, dataid, KEY_SIZE, &rootDirectory, &nBytes);
    } else {
        write_log_direct("init_fs: root is empty\n");

        initialize_element(&rootDirectory, true);

        //Generate a key for rootDirectory and update the root object, and rootDirectory.
        uuid_generate(root_object.id);
        memcpy(rootDirectory.uuid, root_object.id, KEY_SIZE);

        write_log_direct("init_fs: storing thing\n");
        put_record(&root_object.id, &rootDirectory, sizeof(struct fcb));

        write_log_direct("init_fs: storing updated root\n");
        //Store root object.
        rc = store_root();
        if (rc != UNQLITE_OK) {
            error_handler(rc);
        }
    }
}

void shutdown_fs() {
    unqlite_close(pDb);
}

#ifndef IS_LIB

int main(int argc, char *argv[]) {

    // Run tests.
    //test_endpoint();

    int fuserc;
    struct newfs_state *newfs_internal_state;

    //Setup the log file and store the FILE* in the private data object for the file system.
    newfs_internal_state = malloc(sizeof(struct newfs_state));
    newfs_internal_state->logfile = init_log_file();

    //Initialise the file system. This is being done outside of fuse for ease of debugging.
    init_fs();

    fuserc = fuse_main(argc, argv, &newfs_oper, newfs_internal_state);

    //Shutdown the file system.
    shutdown_fs();

    return fuserc;
}

#endif