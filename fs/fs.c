#include "fs.h"

unqlite *pDb;

unqlite_int64 root_object_size_value = sizeof(struct rootS);

unqlite *pDb;
struct rootS root_object;
int root_is_empty;

FILE *logfile;

FILE *init_log_file(){
    
    //Open logfile.
    logfile = fopen("newfs.log", "w");
    if (logfile == NULL) {
		perror("Unable to open log file. Life is not worth living.");
		exit(EXIT_FAILURE);
    }
    //Use line buffering.
    setvbuf(logfile, NULL, _IOLBF, 0);
    return logfile;
}

void write_log(const char *format, ...){
    va_list ap;
    va_start(ap, format);
#ifndef IS_LIB
    vfprintf(NEWFS_PRIVATE_DATA->logfile, format, ap);
#endif
}

void write_log_direct(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfile, format, ap);
}

void error_handler(int rc){
	if( rc != UNQLITE_OK ){
		const char *zBuf;
		int iLen;
		unqlite_config(pDb,UNQLITE_CONFIG_ERR_LOG,&zBuf,&iLen);
		if( iLen > 0 ){
			write_log_direct("error_handler: %s\n",zBuf);
		}
		if( rc != UNQLITE_BUSY && rc != UNQLITE_NOTIMPLEMENTED ){
			/* Rollback */
			unqlite_rollback(pDb);
		}
		exit(rc);
	}
}

void print_id(uuid_t *id){
 	size_t i; 
    for (i = 0; i < sizeof *id; i ++) {
        printf("%02x ", (*id)[i]);
    }
}

//Initialise the store. If no root object is found, create one and write it to the store.
void init_store(){
	int rc;
	write_log_direct("init_store\n");
	// Open the database.
	rc = unqlite_open(&pDb,DATABASE_NAME,UNQLITE_OPEN_CREATE);
	if( rc != UNQLITE_OK ){ error_handler(rc); }

	// Does root already exist?
	rc = fetch_root();
	if(rc==UNQLITE_NOTFOUND){
		write_log_direct("init_store: root object was not found\n");
		// Set the id in the root object to be zero and store it.
		uuid_clear(ROOT_OBJECT_ID);
		store_root();
		write_log_direct("init_store: root object created and stored\n");
		// Set global root_is_empty value to 1.
		root_is_empty = 1;
    }else{
    	if(rc==UNQLITE_OK){
			write_log_direct("init_store: root object was found\n");
	 		// If the id in the root object is null then set global root_is_empty value to 1.
	 		uuid_t nullid;
	 		uuid_clear(nullid);
	 		if(uuid_compare(nullid,ROOT_OBJECT_ID)==0){
	 			write_log_direct("init_store: root object found to be empty\n");
	 			root_is_empty = 1;
	 		}else{
	 			write_log_direct("init_store: root object found to be not empty\n");
		 		// Set root_is_empty to 0.
		 		root_is_empty = 0;
		 	}
		 }else{
		 	error_handler(rc);
		 }
    }
}

//Fetch the root object.
int fetch_root(){
	return unqlite_kv_fetch(pDb,ROOT_OBJECT_KEY,ROOT_OBJECT_KEY_SIZE,&root_object,ROOT_OBJECT_SIZE_P);
}

//Store the root object.
int store_root(){
	return unqlite_kv_store(pDb,ROOT_OBJECT_KEY,ROOT_OBJECT_KEY_SIZE,&root_object,ROOT_OBJECT_SIZE);
}

