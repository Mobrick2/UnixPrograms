/*
 *Name tcp -- trivialll copy a file
 *The tcp utility copies the contents of the source to target.
 *If target is a directory, tcp will copy source into this directory.
 *Examples:
 *1)  tcp file1 file2
 *2)  tcp file1 dir
 *3)  tcp file1 /dir/file2
 *4)  tcp file1 dir/subdir/subsubdir/file2
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef BUFFSIZE
#define BUFFSIZE 32768
#endif

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

char  *
getfilename(char *pathname);

int 
copyfile( int fd1, int fd2);

char *
getdirpath(char *pathname);

int 
cmpabspath(char * path1, char * path2);

int 
main(int argc, char ** argv){
    
   //Check if the argc number is right or not
   if ( argc != 3 ){
      fprintf(stderr, "usage: %s source target\n", argv[0]);
      exit(EX_USAGE);
   }
   
   //Check if argv[1] is a directory
   
   DIR *dp1;
   if ( (dp1 = opendir(argv[1])) != NULL ){
      fprintf(stderr, "tcp: can't copy directory %s\n",argv[1]);
      closedir(dp1);
      exit(EX_DATAERR);
      
   }
   
   
   //Setup the mode for the new file
   mode_t mode;
   mode = (S_IRWXU | S_IRWXG) & ~umask(0);
   //If argv[1] is not directory, try to open the "source" file
   int fd1, fd2;
   if ( (fd1 = open(argv[1], O_RDONLY | O_CREAT, mode)) == -1 ){
      //open source file failed
      fprintf(stderr, "tcp: can't open %s: %s\n", argv[1],
  			strerror(errno));
      exit(EX_NOINPUT);

   }
    
   //Get the source file dirpath
   char * dirpath1;
   if ( (dirpath1 = getdirpath(argv[1])) == NULL){
       exit(EX_USAGE);
   }
   //Get the source file name
   char * fn;
   if ( (fn = getfilename(argv[1])) == NULL )
      exit(EX_DATAERR);
   
   //Check the target file
   DIR *dp2;
   //If the argv[2] is a directory     
   if ( (dp2 = opendir(argv[2])) != NULL ){
        /*
	 *If the source file doen't exist in the directory,then
         *copy it to the directory
         */
        //if their absolute path is same, cannot copy
        if ( cmpabspath(dirpath1,argv[2]) == 0){
	   fprintf(stderr, "tcp: file %s existed in %s\n",fn,argv[2]);
 	   exit(EX_CANTCREAT);
	}
        
        char *pathname;
        //Get the target pathname,+2 cause '\0' or/and '\'
        if ( (pathname = malloc(strlen(fn)+strlen(argv[2])+2)) ==
					NULL){
           fprintf(stderr, "tcp: system error : %s\n",strerror(errno));
	   exit(EX_OSERR);
        } else{
           char *slash;
           slash = strrchr(argv[2],'/');
           if ( slash != NULL && strlen(slash) == 1 ){
	      strcpy(pathname,argv[2]);
	      strcat(pathname,fn);
	   } else{
	      char *c = "/";
              strcpy(pathname,argv[2]);
              strcat(pathname,c);
              strcat(pathname,fn);
	   }
	}
       
	//Try to open a new file
        if ( (fd2 = open(pathname, O_RDWR | O_CREAT | O_TRUNC,
						 mode)) == -1 ){
	     fprintf(stderr, "tcp: can't create %s : %s\n",fn,
                                                strerror(errno));
             exit(EX_CANTCREAT);
	}
        
	//copy the content from the source to the new created file
        if ( copyfile(fd1,fd2) == -1 )
	   exit(EX_CANTCREAT); 

        (void) closedir(dp2);
	free(pathname);
        exit(EX_OK);
   }     
   
   //Get the target file dirpath
   char * dirpath2;
   if ( (dirpath2 = getdirpath(argv[2])) == NULL){
       exit(EX_USAGE);
   }
   
   //Get the target file name
   char * fn2;
   if ( (fn2 = getfilename(argv[2])) == NULL )
      exit(EX_DATAERR);

   //If the argv[2] is not a directory
   //absolute path and file name are both same, cannot copy
   if ((cmpabspath(dirpath1,dirpath2) == 0) && (strcmp(fn,fn2) == 0)){
       fprintf(stderr, "tcp: file %s existed in %s\n",fn,dirpath2);
       exit(EX_CANTCREAT);
   }

   //absulute path or file name is different check, if they are the same
   //file
   struct stat * sb1;
   struct stat * sb2;
   if ( (sb1 = malloc(sizeof(stat))) == NULL ||
                             (sb2 = malloc(sizeof(stat))) == NULL){
         fprintf(stderr, "tcp: system error : %s\n",strerror(errno));
         exit(EX_OSERR);
   }

   if ((fd2 = open(argv[2], O_RDONLY)) != -1 ){
      
      //Deal with the symbol link issue, the two files in the same
      //directory target is the simbol link of source
      if ( fstat(fd1,sb1) == -1 || fstat(fd2,sb2) == -1 ){
         fprintf(stderr, "tcp: system error : %s\n",strerror(errno));
         exit(EX_OSERR);
      }

      if ((int)sb1->st_dev == (int)sb2->st_dev &&
                             (int)sb1->st_ino == (int)sb2->st_ino ){
          fprintf(stderr, "tcp: can't copy %s to itself\n", fn);
          exit(EX_USAGE);
      }
      //close it before reopen it
      (void)close(fd2);
   }
   
   //Open the file again or Create a new file then copy
   if ((fd2 = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, mode)) == -1 ){
        fprintf(stderr, "tcp: can't open or create %s : %s\n",argv[2],
                                        strerror(errno));
        exit(EX_CANTCREAT);
   }
   if ( copyfile(fd1,fd2) == -1)
	exit(EX_CANTCREAT); 
   close(fd1);
   close(fd2);
   exit(EX_OK);
}

/*
 *getfilename
 *if user input a relative pathname or absolute pathname, this 
 *functions get the file name that user is goint to copy
 *
 */
char *
getfilename(char *pathname){
   char *slash;
   slash = strrchr(pathname,'/');
   if ( slash == NULL )
      return pathname;
   
   return  ++slash;
}

/*
 *Copy the content from fd1 to file fd2
 *And close the file if copy successfully
 *sucess return 0, fail return -1
 */
int copyfile(int fd1, int fd2){
   int n;
   char buf[BUFFSIZE];
   //copy the content from the source to the new created file
   while ((n = read(fd1,buf,BUFFSIZE)) > 0){
        if (write(fd2, buf, n) != n) {
           fprintf(stderr, "tcp: write error : %s\n",
                                       strerror(errno));
           return -1;
        }
   }

   (void) close(fd1);
   (void) close(fd2);
   return 0 ;
}

/*
 *Get the directory path of the source file
 *
 */
char *
getdirpath(char * pathname){
   
   char *slash;
   slash = strrchr(pathname,'/');
   
   if ( slash == NULL )
      return ".";
   
   char * temp;
   slash++ ;
   if ( (temp = malloc(strlen(pathname)-strlen(slash) + 1)) ==
						NULL){
      fprintf(stderr, "tcp: system error : %s\n",strerror(errno));
      return NULL;
   }

   strncpy(temp, pathname, strlen(pathname)-strlen(slash));
   return temp; 
}

/*
 *cmpabspath
 *If file1 and file 2 has same absolute path, return 0
 *otherwise !=0
 */

int 
cmpabspath(char * path1, char * path2){
   char * rp1 , *rp2;
   char actualpath[PATH_MAX+1];
   char actualpath2[PATH_MAX+1];
   rp1 = realpath(path1,actualpath);
   rp2 = realpath(path2,actualpath2);
   
   return strcmp(rp1,rp2);
}
