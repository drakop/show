/****************************************************************************
	   b_print.c: Prints the B+ tree created by b_plus.c
			    Georgios Drakopoulos
****************************************************************************/

#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#define MACHINE_16  /*use MACHINE_xx to specify an architecture of xx bits*/

/*define machine-independent unsigned variable types*/
#if defined(MACHINE_16)  /*suitable for PC's*/
  #define WORD_T_TYPE "%u"  /*input-size modifier for ...printf()*/
  #define WORD_T_MAX 65535  /*the maximum value of a word_t variable*/
  #define WORD_T_LSB 0x0001  /*the least significant bit of a word_t value*/
  typedef unsigned char byte_t;  /*8-bit unsigned quantity*/
  typedef unsigned int word_t;  /*16-bit unsigned quantity*/
#elif defined(MACHINE_32)  /*suitable for UNIX servers diogenis and zenon*/
  #define WORD_T_TYPE "%hu"  /*input-size modifier for ...printf()*/
  #define WORD_T_MAX 65535
  #define WORD_T_LSB 0x0001
  typedef unsigned char byte_t;
  typedef unsigned short word_t;
#else
  #error Unsupported architecture or MACHINE_xx not defined.
#endif

#define FILE_BUFFER_SIZE 128  /*buffer size for file name*/
#define WORD_BUFFER_SIZE 8  /*buffer size for a word_t variable*/

#define NO_BLOCK -1L  /*value indicating end of path in the tree*/

#define TREE_ORDER 4  /*the order of the B+ tree*/

/*specify the domain and the range of the boolean type*/
typedef enum { false=0,true=1 } boolean_t;

/*define the structure of a B+ tree node*/
typedef struct
{
  boolean_t is_leaf;  /*is the current node a leaf?*/
  word_t keys_used;  /*indicates how many keys are used*/
  word_t key[TREE_ORDER];  /*the keys for the search*/
  long block[TREE_ORDER+1];  /*the block of the children*/
  long parent_block;  /*the block of the parent*/
} node_t;

/*options to initialize the B+ tree*/
typedef struct
{
  char name[FILE_BUFFER_SIZE];  /*buffer that contains the file name*/
  boolean_t file_exists;  /*true if exists,false if must be created*/
  FILE *iop;  /*the pointer to B+ tree index file returned by tree_open()*/
  node_t *p;  /*pointer to current node in memory*/
} options_t;

/*header information for the B+ tree file*/
typedef struct
{
  size_t header_size;  /*the size of the header_t in bytes*/
  size_t block_size;  /*the size of node_t in bytes*/
  word_t tree_order;  /*the order of the stored tree*/
  long root_block;  /*the block of the root*/
} header_t;

/****************************************************************************
		      main function-argument parsing
   -INPUT: The index file name.
   -OUTPUT: A symbolic value defined in <stdlib.h>
****************************************************************************/
static void open_b_plus_tree(options_t *const opt,header_t *const h);
static void print_b_plus_tree(options_t *const opt,header_t *const h);
static void close_b_plus_tree(options_t *const opt);
static void allocate_block(options_t *const opt,header_t *const h);
static void deallocate_block(options_t *const opt);
static void error(const char *const format,...);

int main(int argc,char *argv[]);
int main(int argc,char *argv[])
{
  options_t options;  /*struct containing the tree's options*/
  header_t header;  /*the header of the index file*/

  /*initialize the options struct*/
  options.file_exists=true;
  options.iop=NULL;
  options.p=NULL;

  if(signal(SIGINT,SIG_IGN)==SIG_ERR)  /*disable Ctrl-C interrupts*/
    error("%s","Cannot install interrupt handler.\n");
  if(argc!=2)
    error("%s","Syntax: b_print <index file name>\n");
  else
  {
    strcpy(options.name,*++argv);
    open_b_plus_tree(&options,&header);
    allocate_block(&options,&header);
    print_b_plus_tree(&options,&header);
    deallocate_block(&options);
    close_b_plus_tree(&options);
  }
  return EXIT_SUCCESS;
}

/****************************************************************************
	error: Prints a message in stderr and quits the program.
   -INPUT: The error message.
   -OUTPUT: A symbolic value defined in <stdlib.h>
****************************************************************************/
static void error(const char *const format,...)
{
  va_list arg_ptr;  /*pointer to argument list*/

  va_start(arg_ptr,format);
  if(format==NULL)
    fprintf(stderr,"%s","An unknown error has occured.\n");
  else vfprintf(stderr,format,arg_ptr);
  exit(EXIT_FAILURE);
  va_end(arg_ptr);
}

/****************************************************************************
   allocate_block: Reserves a memory block equal to block size stored in
		    index file header (h->block_size).
   -INPUT: A constant pointer to an options_t struct and a constant pointer
	   to a header_t struct which contains the exact block size.
   -OUTPUT: None.
****************************************************************************/
static void allocate_block(options_t *const opt,header_t *const h)
{
  if(opt==NULL||h==NULL)
    error("%s","Null input pointer assignment.\n");
  if((opt->p=(node_t *)malloc(h->block_size))==NULL)
    error("%s","Insufficient memory to run program.\n");
  return;
}

/****************************************************************************
      deallocate_block: Frees the memory reserved by allocate_block().
   -INPUT: A constant pointer to an options_t struct.
   -OUTPUT: None.
****************************************************************************/
static void deallocate_block(options_t *const opt)
{
  if(opt==NULL)
    error("%s","Null input pointer assignment.\n");
  free(opt->p);
  opt->p=NULL;  /*just a precaution*/
  return;
}
/****************************************************************************
    open_b_plus_tree: Opens the index file,assigns the file pointer of the
    the corresponding stream to a member of an options_t struct and places
		the index file header to a header_t struct.
   -INPUT: A constant pointer to an options_t struct and a constant pointer
	   to a header_t struct.
   -OUTPUT: None.
****************************************************************************/
static void open_b_plus_tree(options_t *const opt,header_t *const h)
{
  if(opt==NULL||h==NULL)
    error("%s","Null input pointer assignment.");
  if((opt->iop=fopen(opt->name,"rb"))==NULL)
    error("Cannot open index file %s.\n",opt->name);
  if(fread((void *)h,sizeof(header_t),1,opt->iop)!=1)
    error("Cannot read from index file %s.\n",opt->name);
  return;
}

/****************************************************************************
     print_b_plus_tree: Prints sequentially the nodes of the B+ tree.
   -INPUT: A constant pointer to an options_t struct and a constant pointer
	   to a header_t struct.
   -OUTPUT: None.
****************************************************************************/
static void print_b_plus_tree(options_t *const opt,header_t *const h)
{
  word_t index;

  if(fseek(opt->iop,h->block_size,SEEK_SET)!=0)
    error("Cannot move to root block of index file %s.\n",opt->name);
  while(fread(opt->p,h->block_size,1,opt->iop)==1)
  {
    fprintf(stdout,">Keys in node:" WORD_T_TYPE "\n",opt->p->keys_used);
    fprintf(stdout,"%s",(opt->p->is_leaf==true)?">Leaf.\n":">Node.\n");
    if(opt->p->parent_block==NO_BLOCK)
      fprintf(stdout,"%s",">Current node is the root of the B+ tree.\n");
    else fprintf(stdout,"Parent block:%ld.\n",opt->p->parent_block);
    for(index=0;index<opt->p->keys_used;++index)
      fprintf(stdout,WORD_T_TYPE " ",opt->p->key[index]);
    fputc('\n',stdout);
    for(index=0;index<=opt->p->keys_used;++index)
      if(opt->p->block[index]==NO_BLOCK)
	fprintf(stdout,"%s","<nip>");
      else fprintf(stdout,"%ld ",opt->p->block[index]);
    fputc('\n',stdout);
    fprintf(stdout,"%s","\nPress enter to continue...");
    fgetc(stdin);
    fflush(stdout);
  }
  return;
}

/****************************************************************************
  close_b_plus_tree: Closes the file stream assigned by open_b_plus_tree().
   -INPUT: A constant pointer to an options_t struct.
   -OUTPUT: None.
****************************************************************************/
static void close_b_plus_tree(options_t *const opt)
{
  if(opt==NULL)
    error("%s","Null input pointer assignment.\n");
  if(opt->iop!=NULL&&fclose(opt->iop)==EOF)
    error("Cannot close index file %s.\n",opt->name);
  return;
}
