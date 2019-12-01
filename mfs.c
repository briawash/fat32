
// Brianca Washington
// 1001132562
// CSE-3320-001



#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
 
#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5 
#define BPB_BytesPerSec_Offest 11
#define BPB_BytesPerSec_Size 2

#define BPB_SecPerClus_Offest 13
#define BPB_SecPerClus_Size 1

#define BPB_RootEntCnt_Offest 17
#define BPB_RootEntCnt_Size 2

#define BPB_RsvdSecCnt_Offest 14
#define BPB_RsvdSecCnt_Size 2

#define BPB_NumFATs_Offest 16
#define BPB_NumFATs_Size 1

#define BPB_FATSz32_Offest 36
#define BPB_FATSz32_Size 4

#define BS_VolLab_Offest 71
#define BS_VolLab_Size 11

struct __attribute__((__packed__))DirectoryEntry
{
	char DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t Unused1[8];
	uint16_t DIR_FirstClusterHigh;
	uint8_t Unused[4];
	uint16_t DIR_FirstClusterLow;
	uint32_t DIR_FileSize;
	
};
// A linked list node 
struct  OpenNodes
{ 
	//open file fpointer
	//name of file
    FILE *fp2;
    char name[12];

    struct OpenNodes *next;

}OpenNodes; 

// Filepointer for Fat32sys
FILE *fp;

uint16_t BPB_BytesPerSec;
uint8_t  BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t  BPB_NumFATs;
uint32_t BPB_FATSz32;
uint16_t BPB_RootEntCnt;
int32_t FirstDataSector;
int32_t FirstSectorCluster;
int32_t RootDirSector;
uint32_t RootClusAddress;
uint32_t CurrentClusAddress;
uint32_t PrevClusAddress;
char BS_VolLab[11];
struct DirectoryEntry dir[16];
struct OpenNodes* head = NULL;


//assuming this will be what use for read
// also how we change directories basically
int16_t nextLb(uint32_t sector)
{
	uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ( sector *4 );
	int16_t val;
	fseek(fp, FATAddress, SEEK_SET);
	fread(&val,2,1,fp);
	return val;
}
// LBAToOffset calculates the address to look up
//and returns it
int LBAToOffset(int32_t sector)
{
 	return ((sector-2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 *BPB_BytesPerSec);
}
// Open opens a file and createsa new node 
//  to be added to the list of nodes of
void Open( struct OpenNodes **current, char * filename)
{
// Return error message if filename is not included with open command
    if(filename == NULL)
    {
        printf("File name not included with open command.\n");
        return;
    }
	// the list is empty so add a new node to the 
 	//head
    if (head == NULL)
    {
	    (*current) = (struct OpenNodes*)malloc(sizeof(OpenNodes));
	    (*current)->fp2=fopen(filename, "r");
	 	strcpy((*current)->name, filename);
	 	(*current)->next = NULL;


	 	return;
 	}

	//find the end of the linked list
 	if ((*current)!= NULL)
    {
        while((*current)->next!=NULL)
 	        (*current)= (*current)->next;
    }
 	//create new node and add it on
    struct OpenNodes* nextNode= (struct OpenNodes*)malloc(sizeof(OpenNodes));
 	strcpy(nextNode->name,filename);
 	
 	nextNode->fp2=fopen(filename, "r");
 	
 	(*current)->next=nextNode;
 	(*current)=(*current)->next;
 	(*current)->next=NULL;
}

/**/
void initial()
{
 //BPB_BytesPerSec
  fseek(fp, BPB_BytesPerSec_Offest,SEEK_SET);
  fread(&BPB_BytesPerSec, BPB_BytesPerSec_Size, 1, fp);
  

  fseek(fp, BPB_SecPerClus_Offest,SEEK_SET);
  fread(&BPB_SecPerClus, BPB_SecPerClus_Size, 1, fp);


  fseek(fp, BPB_RsvdSecCnt_Offest,SEEK_SET);
  fread(&BPB_RsvdSecCnt, BPB_RsvdSecCnt_Size, 1, fp);


  fseek(fp, BPB_NumFATs_Offest,SEEK_SET);
  fread(&BPB_NumFATs, BPB_NumFATs_Size, 1, fp);


  fseek(fp, BPB_RootEntCnt_Offest,SEEK_SET);
  fread(&BPB_RootEntCnt, BPB_RootEntCnt_Size, 1, fp);


  fseek(fp, BPB_FATSz32_Offest, SEEK_SET);
  fread(&BPB_FATSz32, BPB_FATSz32_Size, 1, fp);
  
  fseek(fp, BS_VolLab_Offest,SEEK_SET);
  fread(&BS_VolLab, BS_VolLab_Size, 1, fp);

}

// The CD command takes in a folder or ".."
// changes it into expanded from then changes the 
// current cluster address and then updates the 
// previous cluster address
void CD(char *location)
{
    char adj[12],adj1[12],*final;
	char name[12];
	int i;
	int found;
	int file_size;
    uint32_t CLusterNumber;

   uint32_t offset;
	//if we want to go into another inner folder
	if(strcmp(location, "../")==0)
    {
		//
		final=strtok(location,"/");
		final=strtok(NULL,".");
        memset( adj, ' ', 12 );

        strncpy(adj,final,strlen(final));
   
	    final=strtok(NULL,"." );
	    strncpy(adj1,final,strlen(final));
	    adj[8]=adj1[0];
	    adj[9]=adj1[1];
	    adj[10]=adj1[2];
	    adj[11]='\0';

	    for(i=0;i<11;i++)
        {
	        if(adj[i]>91 && adj[i]<123)
	 	        adj[i]=toupper(adj[i]);
	    }

        fseek(fp, PrevClusAddress,SEEK_SET);

		for(i=0; i<16; i++)
			fread(&dir[i], sizeof(struct DirectoryEntry),1,fp);
	
	    for(i=0;i<16;i++)
        {
		    memcpy(name, dir[i].DIR_Name,11);
			name[11]='\0';
		
			if(strcmp(name, adj)==0)
	            found =i;
        }

		PrevClusAddress = RootClusAddress;
    }
	
    else if(strcmp(location, "..")==0){
    
		fseek(fp,PrevClusAddress,SEEK_SET);
		PrevClusAddress=RootClusAddress;
    }
	

	else
	{
		// change the  filename	
		final=strtok(location,".");
        memset( adj, ' ', 12 );
        strncpy(adj,final,strlen(final));
        adj[strlen(final)]='\0';
	    for(i=0;i<11;i++)
        {
	        if(adj[i]>91 && adj[i]<123)
	 	        adj[i]=toupper(adj[i]);
	    }
	   


		for(i=0; i<16; i++)
			fread(&dir[i], sizeof(struct DirectoryEntry),1,fp);
	
	    for(i=0;i<16;i++)
        {
		    memcpy(name, dir[i].DIR_Name,11);
			name[11]='\0';
		
			if(strcmp(name, adj)==0)
	            found =i;
        }

		PrevClusAddress = RootClusAddress;
        file_size= dir[i].DIR_FileSize;
		CLusterNumber= (dir[found].DIR_FirstClusterHigh << 16) +(dir[found].DIR_FirstClusterLow);
		offset=LBAToOffset(CLusterNumber);
		fseek(fp, CLusterNumber, SEEK_SET);
		//PrevClusAddress=CurrentClusAddress;
		//CurrentClusAddress=CLusterNumber;
	
	}
}
//Closes the file from the linked list and deletes it
//Will output an error if file was not found
void Close( struct OpenNodes **head1, char * filename)
{
    struct OpenNodes *current=*head1, *temp;
 	while(current!=NULL)
    {
        if(strcmp(current->name,filename)==0)
        {
 	        if(current==*head1)
            {
 				(*head1)=(*head1)->next;
 				fclose(current->fp2);
 				free(current);
 			}
 			else
            {
 				while(strcmp((*head1)->next->name,filename)!=0)
 					(*head1)=(*head1)->next;
 				temp=current;
 				(*head1)->next=current->next;
 				fclose(temp->fp2);
 				free(temp);

 			}
 			return;
 		}
        current=current->next;
 	}
 	
 	printf("File not opened\n");
}
//This file takes the regular and adjusted filename
// and tries to find the file to open and copy the contents into 
//new file
void Get( char *old_filename, char *adjusted_name)
{
 	char info[513];
 	char name[12];
	int found=20,i;
	int file_size,LowCLusterNumber;
	long numBytes,offset;
	//find file is suppose to go
	FILE *newfp;
	//
	info[512]='\0';
	fseek( fp, CurrentClusAddress, SEEK_SET);
	for(i=0; i<512; i++)
        info[i]=' ';
    for(i=0; i<16; i++)
		fread(&dir[i], sizeof(struct DirectoryEntry),1,fp);
	for(i=0;i<16;i++)
	{
        memcpy(name, dir[i].DIR_Name,11);
		name[11]='\0';
		if (strcmp(name, adjusted_name)==0)
	        found =i;
    }
    if (found==20)
		perror("File not found:"); //go in and read and output file
    else
    {
        file_size= dir[found].DIR_FileSize;
		 
		LowCLusterNumber= dir[found].DIR_FirstClusterLow; 
		offset=LBAToOffset(LowCLusterNumber);
		fseek(fp, offset, SEEK_SET);
		newfp=fopen(old_filename,"w");
		 
		while(file_size >512)
        {
	        fread(info, 512,1,fp);
		 	
		 	fwrite(info, 512,1,newfp);
		 	file_size= file_size-512;

		// find the new logical block
		    LowCLusterNumber= nextLb(LowCLusterNumber);
		 	
            if(LowCLusterNumber!= -1)
            {
		 	    offset=LBAToOffset(LowCLusterNumber);
		        fseek(fp,offset, SEEK_SET);
            }
		 	
            else
		        break;
        }
	
		for(i=0; i<512; i++)
			info[i]=' ';
	
		if (file_size >0)
        {	
		    fread(info, file_size,1,fp);
		 	info[file_size]='\0';
		 	fwrite(info, file_size,1 ,newfp);
		}
    }
	fclose(newfp);
}
// Reads a certain portion of the file depending on 
//if the file is readble or the requested amount is over what
// the file size
void Read(char *filename, int position, int amount)
{
    char info[513];
 	char name[12];
	int found=20,i;
	int file_size,LowCLusterNumber;
	long numBytes,offset;
//find file is suppose to go
	fseek( fp, CurrentClusAddress, SEEK_SET);
	info[512]='\0';
	
    for(i=0;i<16;i++)
    {	
        memcpy(name, dir[i].DIR_Name,11);
		name[11]='\0';

		if (strcmp(name, filename)==0)
	        found =i;
    }

	if (found==20)
		perror("File not found:");

	//go in and read and output file

	else
    {
        file_size= dir[found].DIR_FileSize;
		 
		LowCLusterNumber= dir[found].DIR_FirstClusterLow ; 
		offset=LBAToOffset(LowCLusterNumber) + position;
		fseek(fp, offset, SEEK_SET);
		 
		while(amount >512)
        {
		    fread(&info, 512,1,fp);
		 	printf("%s",info );
		 	amount= amount-512;

		// find the new logical block
		 	LowCLusterNumber= nextLb(LowCLusterNumber);
		 	if(LowCLusterNumber!= -1)
            {
		 		offset=LBAToOffset(LowCLusterNumber);
		 		fseek(fp,offset, SEEK_SET);
		 	}
		 	
            else
		 		break;

		 }
          
		 for(i=0; i<512; i++)
			info[i]=' ';

         if (amount >0)
         {
		 	fread(&info, amount,1,fp);
		 	printf("%s\n",info );
		 	
		 }
    }
}

// shows the directory of the current cluster
void ls()
{
    int i=0;
	char name[12];
	
    fseek( fp, CurrentClusAddress, SEEK_SET);
	
    for  (i=0; i<16; i++)
		fread(&dir[i], sizeof(struct DirectoryEntry),1,fp);
	
    for  (i=0; i<16; i++)
    {
		char name[12];
		memcpy(name, dir[i].DIR_Name,11);
		name[11]='\0';
		if(dir[i].DIR_Attr== 0x20|| dir[i].DIR_Attr==0x10||dir[i].DIR_Attr==0x01)
			printf("%s\n", name );
	}
}

//Info_Command is for printing the info about the system
// It offers the info in hex then decimal
void Info(void)
{
	// outputting the information
	printf("BPB_BytesPerSec: %xh %d \n", BPB_BytesPerSec, BPB_BytesPerSec);
	printf("BPB_SecPerClus: %xh %d \n", BPB_SecPerClus, BPB_SecPerClus);
	printf("BPB_RsvdSecCnt: %xh %d \n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
	printf("BPB_NumFATs: %xh %d \n", BPB_NumFATs, BPB_NumFATs);
	printf("BPB_FATSz32: %xh %d \n", BPB_FATSz32, BPB_FATSz32);

}
// Prints out the info about a file if found
void Stat(char* filename)
{
	int i,found=20;
	int numBytes;
	fseek( fp, RootClusAddress, SEEK_SET);
	for(i=0; i<16; i++)
		fread(&dir[i], sizeof(struct DirectoryEntry),1,fp);
	
	for(i=0;i<16;i++)
	{	
		char name[12];
		memcpy(name, dir[i].DIR_Name,11);
		name[11]='\0';
		
		if (strcmp(name, filename)==0)
		{	
			found =i;
			numBytes=dir[i].DIR_FileSize;
		}
    }
	
    if (found==20)
		perror("File not found:");

	else
    {
		printf("Starting Cluster: %xh \n", dir[found].DIR_FirstClusterLow);
		printf("Attributes : %x h\n", dir[found].DIR_Attr);
	}
}

int main()
{
	char* original;
    char adj[12];
	char adj1[12];
	char *final;
	int period, flag=0;
	int i=0;
	fp=fopen("fat32.img", "r");
	char *adjusted_name;
   	adjusted_name = (char *)malloc(11);	
   	initial();


	RootClusAddress = (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec) + (BPB_RsvdSecCnt * BPB_BytesPerSec);
	CurrentClusAddress=RootClusAddress;
	fseek( fp, RootClusAddress, SEEK_SET);
    for  (i=0; i<16; i++)
		fread(&dir[i], sizeof(struct DirectoryEntry),1,fp);

    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

    while( 1 )
    { 
		// Print out the msh prompt
		printf ("mfs> ");

		// Read the command from the commandline.  The
		// maximum command that will be read is MAX_COMMAND_SIZE
		// This while command will wait here until the user
		// inputs something since fgets returns NULL when there
		// is no input
		while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

		/* Parse input */
		char *token[MAX_NUM_ARGUMENTS];

		int   token_count = 0;                                 
		                                                       
		// Pointer to point to the token
		// parsed by strsep
		char *arg_ptr;                                         
		                                                       
		char *working_str  = strdup( cmd_str );                

		// we are going to move the working_str pointer so
		// keep track of its original value so we can deallocate
		// the correct amount at the end
		char *working_root = working_str;

        // Tokenize the input stringswith whitespace used as the delimiter
		while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
		          (token_count<MAX_NUM_ARGUMENTS))
		{
		  token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
		  if( strlen( token[token_count] ) == 0 )
		  {
		    token[token_count] = NULL;
		  }
		    token_count++;
		}
        free( working_root );

    // ls command
	   if (strcmp(token[0], "ls")==0)
	   {
	       ls();
	   }
    // Open command
	   else if (strcmp(token[0], "open")==0)
	   {
	       Open(&head,token[1]);
	   }
    // Close command
	   else if (strcmp(token[0], "close")==0)
	   {
	       Close(&head,token[1]);
	   }
    // Info command
	   else if (strcmp(token[0], "info")==0)
	   {
	       Info();
	   }
    // Change directory
	   else if (strcmp(token[0], "cd")==0)
	   {
	   		
	       CD(token[1]);
	   }
    
    // Get command
		else if (strcmp(token[0], "get")==0)
		{
		    final=strtok(token[1],".");
		    memset( adj, ' ', 12 );

		    strncpy(adj,final,strlen(final));
	   
			final=strtok(NULL,"." );
			strncpy(adj1,final,strlen(final));
			adj[8]=adj1[0];
			adj[9]=adj1[1];
			adj[10]=adj1[2];
			adj[11]='\0';

			for(i=0;i<11;i++)
		    {
		 	    if(adj[i]>91 && adj[i]<123)
		            adj[i]=toupper(adj[i]);
			}

		    Get(token[1], adj);
	   }
   
    // Read command
	   else if (strcmp(token[0], "read")==0)
	   {
		   final=strtok(token[1],".");
	  	   memset( adj, ' ', 12 );
	  	   strncpy(adj,final,strlen(final));
	   
		   final=strtok(NULL,"." );
		
		   strncpy(adj1,final,strlen(final));
		   adj[8]=adj1[0];
		   adj[9]=adj1[1];
		   adj[10]=adj1[2];
		   adj[11]='\0';

		   for(i=0;i<11;i++)
		   {
			   if(adj[i]>91 && adj[i]<123)
			       adj[i]=toupper(adj[i]);
		   }

		   Read(adj,atoi(token[2]),atoi(token[3]));
	   }
    
    // Stat command
		else if (strcmp(token[0], "stat")==0)
		{
		    
	   	 
	 	    final=strtok(token[1],".");
		    memset( adj, ' ', 12 );

		    strncpy(adj,final,strlen(final));
	   
		    final=strtok(NULL,"." );
			strncpy(adj1,final,strlen(final));
			adj[8]=adj1[0];
			adj[9]=adj1[1];
			adj[10]=adj1[2];
			adj[11]='\0';

			for(i=0;i<11;i++)
		    {
		 	    if(adj[i]>91 && adj[i]<123)
		 		    adj[i]=toupper(adj[i]);
			}

		    Stat(adj);
		}
   
    // Command not found
        else
            printf("Command not found.\n");

    }// End of while loop

// End of program
    return 0;
}
