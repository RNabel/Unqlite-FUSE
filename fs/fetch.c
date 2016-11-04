#include "fs.h"

int main(int argc, char** argv){
	int rc;

	//initialise the log file
	init_log_file();

	init_store();
	
	if(!root_is_empty){
	
		uuid_t *dataid = &(root_object.id);
	
		unqlite_int64 nBytes;  //Data length
		rc = unqlite_kv_fetch(pDb,dataid,KEY_SIZE,NULL,&nBytes);
		if( rc != UNQLITE_OK ){
		  error_handler(rc);
		}
		if(nBytes!=sizeof(struct notafcb)){
			printf("Data object has unexpected size. Doing nothing.\n");
			exit(-1);
		}

		struct notafcb rootDirectory;
	
		// Fetch data.
		unqlite_kv_fetch(pDb,dataid,KEY_SIZE,&rootDirectory,&nBytes);

		printf("key is:\n");
		print_id(dataid);	
		
		printf("\npath: %s\nperm: %04o\ndata: %s\n", rootDirectory.path,rootDirectory.mode & (S_IRWXU|S_IRWXG|S_IRWXO),rootDirectory.data);
	}else{
		printf("Root object was empty. Nothing to fetch.");
	}
	
	//Close our database handle
	unqlite_close(pDb);
}

