#include "fs.h"

int main(int argc, char** argv){
	int rc;
	
	//initialise the log file
	init_log_file();

	init_store();
	
	if(root_is_empty){
		// Create data object id and store it in the root object.
    	uuid_generate(root_object.id);
    	
    	printf("id is:\n");
    	print_id(&(root_object.id));
		printf("\n");
		
		struct notafcb rootDirectory;
		memset(&rootDirectory, 0, sizeof(struct notafcb));
		
		sprintf(rootDirectory.path,"/hello.txt");
		sprintf(rootDirectory.data,"This is the file's contents.");
		
		// see 'man 2 stat' and 'man 2 chmod'
		rootDirectory.mode |= S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH; 
		rootDirectory.mtime = time(0);
		int datasize = sizeof(struct notafcb);
		// Store data object.
		rc = unqlite_kv_store(pDb,&(root_object.id),KEY_SIZE,&rootDirectory,datasize);
	 	if( rc != UNQLITE_OK ){
   			error_handler(rc);
		}
		// Store root object.
		rc = store_root();
	 	if( rc != UNQLITE_OK ){
   			error_handler(rc);
		}
	}else{
		printf("Root object is not empty. Doing nothing.\n");
	}

	unqlite_close(pDb);

	return 0;
}

