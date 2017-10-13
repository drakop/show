/****************************************************************************
		b_plus.c: A implementation of B+ trees in C.
			  Georgios Drakopoulos
****************************************************************************/

#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define MACHINE_16  /*use MACHINE_xx to specify an architecture of xx bits*/

/*define machine-independent unsigned variable types*/
#if defined(MACHINE_16)  /*proper for PC's*/
  #define WORD_T_TYPE "%u"  /*input-size modifier for ...printf()*/
  #define WORD_T_MAX 65535  /*the maximum value of a word_t variable*/
  #define WORD_T_LSB 0x0001  /*the least significant bit of a word_t value*/
  typedef unsigned char byte_t;  /*8-bit unsigned quantity*/
  typedef unsigned int word_t;  /*16-bit unsigned quantity*/
#elif defined(MACHINE_32)  /*proper for UNIX servers diogenis and zenon*/
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

/*specify the available options at the main menu*/
enum ch { CREATE='1',OPEN='2',CLOSE='3',INSERT='4',SEARCH='5',QUIT='0' };

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

typedef enum  /*symbolic names for the various errors*/
{
  SUCCESS=0,
  INV_OPT_PTR=(-1),  /*null pointer to option_t struct*/
  INV_HEADER_PTR=(-2),  /*null pointer to header_t struct*/
  INV_DATA_PTR=(-3),  /*null pointer to value*/
  E_CREATE_FILE=(-4),  /*error while creating index file*/
  E_OPEN_FILE=(-5),  /*error while opening index file*/
  E_CLOSE_FILE=(-6),  /*error while closing index file*/
  E_WRITE_FILE=(-7),  /*error while writing to index file*/
  E_READ_FILE=(-8),  /*error while reading from index file*/
  E_MOVE_FILE=(-9),  /*unable to move within the index file*/
  E_NO_MEMORY=(-10),  /*there is no available memory*/
  E_TREE_EMPTY=(-11),  /*cannot search an empty tree*/
  E_INCOMPATIBLE_VERSION=(-12)  /*incompatible version with data*/
} status_t;

static const char *error_msg[]=
{
  "No error occured.",
  "Null pointer to option struct.",
  "Null pointer to file header struct.",
  "Null pointer to tree data.",
  "Cannot create designated index file.",
  "Cannot open designated index file.",
  "Cannot close designated index file.",
  "Cannot write to designated index file.",
  "Cannot read from designated index file.",
  "Cannot move within designated index file.",
  "Insufficient memory to run program.",
  "The B+ tree is empty.",
  "The tree order of the index file is incompatible with the program."
};

/****************************************************************************
			      main function
			      -input: None.
   -output(to the environemnt): A symbolic value defined in <stdlib.h>.
****************************************************************************/
static status_t insert_value(header_t *h,options_t *opt,word_t value);
static status_t open_tree(options_t *const opt,header_t *const h);
static status_t close_tree(options_t *const opt);
static status_t reallocate_block(options_t *const opt);
static status_t deallocate_block(options_t *const opt);
static status_t read_file_name(options_t *const opt);
static status_t read_word_t(word_t *const value);
static void error(const char *const format,...);
static void display_menu(void);

int main(void);
int main(void)
{
  options_t options;  /*initializing options of B+ tree*/
  header_t header;   /*header of B+ tree*/
  status_t status;  /*status indicator returned by last function*/
  word_t value;
  int choice;


  /*load initial values to both header and options*/
  options.file_exists=false;
  options.p=NULL;
  options.iop=NULL;

  header.tree_order=TREE_ORDER;
  header.block_size=sizeof(node_t);
  header.header_size=sizeof(header_t);
  header.root_block=NO_BLOCK;

  if(signal(SIGINT,SIG_IGN)==SIG_ERR)  /*ignore Ctrl-C signals*/
    error("%s\n","Unable to install user-defined interrupt handler.");
  fprintf(stdout,"B_PLUS ver 1.00 compiled on %s at %s.\n",__DATE__,__TIME__);
  fflush(stdout);
  do
  {
    display_menu();
    fflush(stdin);
    switch(choice=getc(stdin))
    {
      case CREATE:
	close_tree(&options);
	options.file_exists=false;
	read_file_name(&options);
	if((status=reallocate_block(&options))!=SUCCESS)
	  error("%s\n",error_msg[-status]);
	if((status=open_tree(&options,&header))!=SUCCESS)
	  error("%s\n",error_msg[-status]);
	else fprintf(stderr,"File %s has been created.\n",options.name);
	break;
      case OPEN:
	close_tree(&options);
	options.file_exists=true;
	read_file_name(&options);
	if((status=reallocate_block(&options))!=SUCCESS)
	  error("%s\n",error_msg[-status]);
	if((status=open_tree(&options,&header))!=SUCCESS)
	  error("%s\n",error_msg[-status]);
	else fprintf(stderr,"File %s has been opened.\n",options.name);
	break;
      case CLOSE:
	close_tree(&options);
	fprintf(stderr,"File %s has been closed.\n",options.name);
	break;
      case INSERT:
	if(options.iop==NULL)
	  fprintf(stderr,"%s\n","You must open/create a file first.");
	else
	{
	  if((status=read_word_t(&value))!=SUCCESS)
	    error("%s\n",error_msg[-status]);
	  if((status=insert_value(&header,&options,value))!=SUCCESS)
	    error("%s\n",error_msg[-status]);
	}
	break;
      case SEARCH:
	if(options.iop==NULL)
	  fprintf(stderr,"%s\n","You must open/create a file first.");
	else
	{
	  read_word_t(&value);
	}
	break;
      case QUIT:
	close_tree(&options);
	fprintf(stderr,"File %s has been closed.\n",options.name);
	break;
      default:
	fprintf(stderr,"%s\n","Invalid option,try again.");
	break;
    }
  }
  while(choice!=QUIT);
  deallocate_block(&options);
  return EXIT_SUCCESS;
}

/****************************************************************************
       error: Prints an error message in stderr and quits the program.
		       -input: The error message.
    -output(to the environment): A symbolic value defined in <stdlib.h>.
****************************************************************************/
static void error(const char *const format,...)
{
  va_list arg_ptr; /*pointer to argument list*/
  va_start(arg_ptr,format);
  if(format==NULL)
    fprintf(stderr,"%s\n","An unknown error has occured.");
  else vfprintf(stderr,format,arg_ptr);
  exit(EXIT_FAILURE);
  va_end(arg_ptr);
}

/****************************************************************************
	   display_menu: Prints to the user the available options.
			      -input: None.
			      -output: None.
****************************************************************************/
static void display_menu(void)
{
  const char menu[]="\n[1] Create new index file.\n[2] Open existing index\
  \bfile.\n[3] Close current index file.\n[4] Insert a value into current i\
  \b\bndex file.\n[5] Search for a value into current index file.\n[0] Qui\
  \b\bt program.\n\nYour choice:";
  fprintf(stdout,"%s",menu);
  fflush(stdout);
  return;
}

/****************************************************************************
 reallocate_block: Reserves memory for one node (which fits to a disk block)
	of a B+ tree or resizes it to fit current tree's block size.
	  -input: A constant pointer to the B+ tree's options.
	-output: A status_t value indicating success or an error.
****************************************************************************/
static status_t reallocate_block(options_t *const opt)
{
  if(opt==NULL)
    return INV_OPT_PTR;
  if((opt->p=(node_t *)realloc(opt->p,sizeof(node_t)))==NULL)
    return E_NO_MEMORY;
  return SUCCESS;
}

/****************************************************************************
   deallocate_block: Deallocates the memory reserved from allocate_block().
	    -input: A constant pointer to the B+ tree's options.
	  -output: A status_t value indicating sucess or an error.
****************************************************************************/
static status_t deallocate_block(options_t *const opt)
{
  if(opt==NULL)
    return INV_OPT_PTR;
  if(opt->p!=NULL)
    free(opt->p);
  opt->p=NULL;
  return SUCCESS;
}

/****************************************************************************
	    open_tree: Opens/constructs the B+ tree in the disk.
  -input: A constant pointer to B+ tree's options and a constant pointer to
			    the B+ tree's header.
	  -output: A status_t value indicating sucess or an error.
****************************************************************************/
static status_t open_tree(options_t *const opt,header_t *const h)
{
  if(opt==NULL)
    return INV_OPT_PTR;
  if(h==NULL)
    return INV_HEADER_PTR;
  if(opt->file_exists==true)
  {
    if((opt->iop=fopen(opt->name,"r+b"))==NULL)
      return E_OPEN_FILE;
    if(fread(h,sizeof(header_t),1,opt->iop)!=1)
      return E_READ_FILE;
  }
  else
  {
    if((opt->iop=fopen(opt->name,"w+b"))==NULL)
      return E_CREATE_FILE;
    if(fwrite(h,sizeof(header_t),1,opt->iop)!=1)
      return E_WRITE_FILE;
    fflush(opt->iop);
  }
  return SUCCESS;
}

/****************************************************************************
	      close_tree: Closes a file containing a B+ tree.
	   -input: A constant pointer to the B+ tree's options.
	 -output: A status_t value indicating sucess or an error.
****************************************************************************/
static status_t close_tree(options_t *const opt)
{
  if(opt==NULL)
    return INV_OPT_PTR;
  if(opt->iop!=NULL&&fclose(opt->iop)==EOF)
    return E_CLOSE_FILE;
  opt->iop=NULL;  /*just a precaution*/
  return SUCCESS;
}

/****************************************************************************
		insert_value: Inserts a value in B+ tree.
 -input: A pointer to the B+ tree's header,a pointer to the B+ tree's options
	       and a word_t variable (the value to be inserted).
	   -output: A status_t value indicating sucess or an error.
****************************************************************************/
static status_t node_overflow(options_t *const opt,header_t *const h);

static status_t insert_value(header_t *h,options_t *opt,word_t value)
{
  word_t index,new_pos;
  boolean_t insert;

  if(h==NULL)
    return INV_HEADER_PTR;
  if(opt==NULL)
    return INV_OPT_PTR;
  if(value==NULL)
    return INV_DATA_PTR;
  if(h->tree_order>TREE_ORDER)
    return E_INCOMPATIBLE_VERSION;
  if(h->root_block==NO_BLOCK)  /*the tree is initially empty*/
  {
    if(fseek(opt->iop,0L,SEEK_SET)!=0)
      return E_MOVE_FILE;
    h->root_block=(unsigned long)h->header_size;
    if(fwrite(h,sizeof(header_t),1,opt->iop)!=1)
      return E_WRITE_FILE;
    fflush(opt->iop);

    /*initialize root node*/
    opt->p->key[0]=value;
    opt->p->keys_used=1;
    opt->p->parent_block=NO_BLOCK;
    opt->p->is_leaf=false;
    for(index=0;index<=h->tree_order;++index)  /*(tree_order+1) blocks*/
      opt->p->block[index]=NO_BLOCK;
    if(fwrite(opt->p,h->block_size,1,opt->iop)!=1)
      return E_WRITE_FILE;
    fflush(opt->iop);
  }
  else
  {
    fseek(opt->iop,h->root_block,SEEK_SET); /*go to the root*/
    insert=false;
    while(insert==false)
    {
      fread(opt->p,h->block_size,1,opt->iop);
      /*search for the first entry q in node that value<=q*/
      for(new_pos=0;new_pos<opt->p->keys_used;++new_pos)
	if(value<=opt->p->key[new_pos])
	  break;
      if(value==opt->p->key[new_pos])
	insert=true;  /*value exists*/
      else if(opt->p->block[new_pos+1]==NO_BLOCK)  /*no more path to follow*/
	   {
	     ++(opt->p->keys_used);
	     for(index=opt->p->keys_used-1;index>new_pos;--index)
	       opt->p->key[index]=opt->p->key[index-1];
	     opt->p->key[new_pos]=value;
	     for(index=opt->p->keys_used;index>new_pos;--index)
	       opt->p->block[index]=opt->p->block[index-1];
	     opt->p->block[new_pos+1]=NO_BLOCK;
	     fseek(opt->iop,-(long)h->block_size,SEEK_CUR);
	     fwrite(opt->p,h->block_size,1,opt->iop);
	     if(opt->p->keys_used==h->tree_order)
	       node_overflow(opt,h);
	     insert=true;  /*value successfully inserted into the tree*/
	   }
	   else  /*the path continues*/
	   {
	     fseek(opt->iop,opt->p->block[new_pos+1],SEEK_SET);
	   }
    }
  }
  return SUCCESS;
}

/****************************************************************************
	   node_overflow: Implements the overflow in a B+ tree.
  -input: A constant pointer to the B+ tree's options and a constant pointer
		      to the B+ tree's file header.
       -output: A status_t value indicating success or an error
****************************************************************************/
static status_t node_overflow(options_t *const opt,header_t *const h)
{
  word_t q,left_keys,right_keys,index,new_pos;
  long par_block,left_block,right_block;
  static boolean_t initialized=false;
  long temp_block[TREE_ORDER+1];
  word_t temp_key[TREE_ORDER];
  boolean_t overflow;

  if(initialized==false)
  {
    srand((unsigned int)(time(NULL)%RAND_MAX));
    initialized=true;
  }
  q=(rand()>(RAND_MAX>>1U))?(word_t)0:(word_t)1;
  left_keys=(h->tree_order>>1U)-q;
  right_keys=(h->tree_order>>1U)+q-1;
  overflow=true;
  while(overflow==true)
  {
    if(opt->p->parent_block==NO_BLOCK)  /*if the root must break*/
    {
      for(index=0;index<opt->p->keys_used;++index)
	temp_key[index]=opt->p->key[index];
      for(index=0;index<=opt->p->keys_used;++index)
	temp_block[index]=opt->p->block[index];

      /*write left son*/
      opt->p->parent_block=(unsigned long)h->header_size;
      opt->p->keys_used=left_keys;
      for(index=0;index<left_keys;++index)
	opt->p->key[index]=temp_key[index];
      for(index=0;index<=left_keys;++index)
	opt->p->block[index]=temp_block[index];
      fseek(opt->iop,0L,SEEK_END);
      par_block=left_block=ftell(opt->iop);
      fwrite(opt->p,h->block_size,1,opt->iop);
      fflush(opt->iop);

      for(index=0;index<=opt->p->keys_used;++index)
      {
	fseek(opt->iop,opt->p->block[index],SEEK_SET);
	fread(opt->p,h->block_size,1,opt->iop);
	opt->p->parent_block=par_block;
	fseek(opt->iop,-(long)h->block_size,SEEK_CUR);
	fwrite(opt->p,h->block_size,1,opt->iop);
      }

      /*write right son*/
      opt->p->keys_used=right_keys;
      for(index=left_keys+1;index<h->tree_order;++index)
	opt->p->key[index-left_keys-1]=temp_key[index];
      for(index=left_keys+1;index<=h->tree_order;++index)
	opt->p->block[index-left_keys-1]=temp_block[index];
      par_block=right_block=ftell(opt->iop);
      fwrite(opt->p,h->block_size,1,opt->iop);
      fflush(opt->iop);

      for(index=0;index<=opt->p->keys_used;++index)
      {
	fseek(opt->iop,opt->p->block[index],SEEK_SET);
	fread(opt->p,h->block_size,1,opt->iop);
	opt->p->parent_block=par_block;
	fseek(opt->iop,-(long)h->block_size,SEEK_CUR);
	fwrite(opt->p,h->block_size,1,opt->iop);
      }

      /*rewrite the root node*/
      fseek(opt->iop,(unsigned long)h->header_size,SEEK_SET);
      fread(opt->p,h->block_size,1,opt->iop);
      opt->p->keys_used=1,opt->p->parent_block=NO_BLOCK;
      opt->p->key[0]=temp_key[left_keys];
      opt->p->block[0]=left_block,opt->p->block[1]=right_block;
      fseek(opt->iop,-(long)h->block_size,SEEK_CUR);
      fwrite(opt->p,h->block_size,1,opt->iop);
      fflush(opt->iop);

      overflow=false; /*the root has been broken*/
    }
    else
    {
       for(index=left_keys;index<opt->p->keys_used;++index)
	 temp_key[index]=opt->p->key[index];
       for(index=left_keys;index<=opt->p->keys_used;++index)
	 temp_block[index]=opt->p->block[index];
       opt->p->keys_used=left_keys;
       fseek(opt->iop,-(long)h->block_size,SEEK_CUR);
       left_block=ftell(opt->iop);
       fwrite(opt->p,h->block_size,1,opt->iop);
       fseek(opt->iop,0L,SEEK_END);
       right_block=ftell(opt->iop);
       for(index=1;index<=right_keys;++index)
	 opt->p->key[index-1]=temp_key[index];
       for(index=0;index<=right_keys;++index)
	 opt->p->block[index]=temp_block[index];
       opt->p->keys_used=right_keys;
       fwrite(opt->p,h->block_size,1,opt->iop);
       fseek(opt->iop,opt->p->parent_block,SEEK_SET);
       fread(opt->p,h->block_size,1,opt->iop);
       for(new_pos=0;new_pos<opt->p->keys_used;++new_pos)
	 if(*temp_key<opt->p->key[new_pos])
	   break;
       ++(opt->p->keys_used);
       for(index=opt->p->keys_used-1;index>new_pos;--index)
	 opt->p->key[index]=opt->p->key[index-1];
      opt->p->key[new_pos]=*temp_key;
      for(index=opt->p->keys_used;index>new_pos;--index)
	 opt->p->block[index]=opt->p->block[index-1];
      opt->p->block[new_pos+1]=right_block;
      fseek(opt->iop,-(long)h->block_size,SEEK_CUR);
      fwrite(opt->p,h->block_size,1,opt->iop);
      if(opt->p->keys_used<h->tree_order)
	overflow=false;
    }
  }
  return SUCCESS;
}

/****************************************************************************
	 read_file_name: Reads the index file name from stdin.
	  -input: A constant pointer to the B+ tree's options.
	 -output: A status_t value indicating sucess or an error.
****************************************************************************/
static status_t read_file_name(options_t *const opt)
{
  size_t last_char_index;
  if(opt==NULL)
    return INV_OPT_PTR;
  do
  {
    fprintf(stdout,"%s","Enter index file name:");
    fflush(stdout);
    fflush(stdin);
  }
  while(!fgets(opt->name,FILE_BUFFER_SIZE,stdin)||isspace((int)*opt->name));
  if(opt->name[last_char_index=(strlen(opt->name)-1)]=='\n')
    opt->name[last_char_index]='\0';
  return SUCCESS;
}

/****************************************************************************
	read_word_t: Reads a word_t value (in decimal) from stdin.
	       -input: A constant pointer to a word_t variable.
	    -output: A status_t value indicating success or an error.
****************************************************************************/
static status_t read_word_t(word_t *const value)
{
  char buffer[WORD_BUFFER_SIZE];
  if(value==NULL)
    return INV_DATA_PTR;
  do
    do
    {
      fprintf(stdout,"Enter the value (0-"WORD_T_TYPE"):",WORD_T_MAX);
      fflush(stdout);
      fflush(stdin);
    }
    while(!fgets(buffer,WORD_BUFFER_SIZE,stdin)||isspace((int)*buffer));
  while(sscanf(buffer,WORD_T_TYPE,value)!=1);
  return SUCCESS;
}
