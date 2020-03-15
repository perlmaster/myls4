/*********************************************************************
*
* File      : ls4.c
*
* Author    : Barry Kimelman
*
* Created   : October 26, 2019
*
* Purpose   : List file information in the style of ls
*
*********************************************************************/

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<dirent.h>
#include	<string.h>
#include	<time.h>
#include	<string.h>
#include	<stdarg.h>
#include	<getopt.h>
#include	<signal.h>    /* signal name macros, and the signal() prototype */

#define	EQ(s1,s2)	(strcmp(s1,s2)==0)
#define	NE(s1,s2)	(strcmp(s1,s2)!=0)
#define	GT(s1,s2)	(strcmp(s1,s2)>0)
#define	LT(s1,s2)	(strcmp(s1,s2)<0)
#define	LE(s1,s2)	(strcmp(s1,s2)<=0)

#define		MAX_SUBDIRS	128
#define		MAX_FILES	2048

typedef	struct filedata_tag {
	char	*filename;
	int		file_size;
	int		file_nlinks;
	time_t	file_mtime;
	unsigned short int	file_mode;
} FILEDATA;
static	FILEDATA	files_info[MAX_FILES];

#ifdef OLDSTUFF
static	char		*files[MAX_FILES];
static	int			file_sizes[MAX_FILES];
static	int			file_nlinks[MAX_FILES];
static	time_t		file_times[MAX_FILES];
static	unsigned short int		file_modes[MAX_FILES];
#endif

static	char	*srcfiles[] = { "file1.c" , "aaa.c" , "nextfile.c" , "bozo" };
static	int		nsrcfiles = sizeof(srcfiles) / sizeof(char *);

static	char	ftypes[] = {
 '.' , 'p' , 'c' , '?' , 'd' , '?' , 'b' , '?' , '-' , '?' , 'l' , '?' , 's' , '?' , '?' , '?'
};

static char	*perms[] = {
	"---" , "--x" , "-w-" , "-wx" , "r--" , "r-x" , "rw-" , "rwx"
};

static char	*months[12] = { "Jan" , "Feb" , "Mar" , "Apr" , "May" , "Jun" ,
				"Jul" , "Aug" , "Sep" , "Oct" , "Nov" , "Dec" } ;

static	int		opt_d = 0 , opt_t = 0 , opt_s = 0 , opt_R = 0;
static	int		opt_D = 0 , opt_r = 0 , opt_h = 0 , opt_k = 0;
static	int		opt_i = 0 , opt_a = 0 , opt_l = 0 , opt_n = 0;
static	int		num_args;
static	int		num_files = 0;
static	int		file_bytes = 0;
static	int		num_dirs = 0;
static	int		kb = 1 << 10;
static	int		mb = 1 << 20;
static	int		gb = 1 << 30;

extern	int		optind , optopt , opterr;

extern	void	system_error() , quit() , die();
extern	void format_number_with_commas(int number, char *buffer);

/* first, here is the signal handler */
void catch_int(int sig_num)
{
    /* re-set the signal handler again to catch_int, for next time */
    signal(SIGINT, catch_int);
    printf("Don't do that\n");
    fflush(stdout);
} /* end of catch_int */

/* here is another signal handler */
void catch_sig(int sig_num)
{
	 fflush(stderr);
	 fflush(stdout);
    /* re-set the signal handler to default action */
    signal(sig_num, SIG_DFL);
    fprintf(stderr,"Caught signal %d\n",sig_num);
    fflush(stderr);
	 /* abort(); */
	 exit(sig_num);  /* just in case abort() does not work as expected */
} /* end of catch_int */

/* string comparison function for qsort by name */

static int compare_name(const void *arg_s1, const void *arg_s2)
{
	const FILEDATA *f1 = (const FILEDATA *) arg_s1;
	const FILEDATA *f2 = (const FILEDATA *) arg_s2;
			
	return(strcmp(f1->filename, f2->filename));
}

/* integer size comparison function for qsort */

static int compare_size(const void *arg_s1, const void *arg_s2)
{
	const FILEDATA *f1 = (const FILEDATA *) arg_s1;
	const FILEDATA *f2 = (const FILEDATA *) arg_s2;
			
	if ( f1->file_size < f2->file_size ) {
		return -1;
	}
	else {
		if ( f1->file_size == f2->file_size ) {
			return 0;
		}
		else {
			return 1;
		}
	}
}

/* integer time comparison function for qsort */

static int compare_time(const void *arg_s1, const void *arg_s2)
{
	const FILEDATA *f1 = (const FILEDATA *) arg_s1;
	const FILEDATA *f2 = (const FILEDATA *) arg_s2;
			
	if ( f1->file_mtime < f2->file_mtime ) {
		return -1;
	}
	else {
		if ( f1->file_mtime == f2->file_mtime ) {
			return 0;
		}
		else {
			return 1;
		}
	}
}

/*********************************************************************
*
* Function  : debug_print
*
* Purpose   : Display an optional debugging message.
*
* Inputs    : char *format - the format string (ala printf)
*             ... - the data values for the format string
*
* Output    : the debugging message
*
* Returns   : nothing
*
* Example   : debug_print("The answer is %s\n",answer);
*
* Notes     : (none)
*
*********************************************************************/

void debug_print(char *format,...)
{
	va_list ap;

	if ( opt_D ) {
		va_start(ap,format);
		vfprintf(stdout, format, ap);
		fflush(stdout);
		va_end(ap);
	} /* IF debug mode is on */

	return;
} /* end of debug_print */

/*********************************************************************
*
* Function  : usage
*
* Purpose   : Display a program usage message
*
* Inputs    : char *pgm - name of program
*
* Output    : the usage message
*
* Returns   : nothing
*
* Example   : usage("The answer is %s\n",answer);
*
* Notes     : (none)
*
*********************************************************************/

void usage(char *pgm)
{
	fprintf(stderr,"Usage : %s [-hFgiDdtsr]\n\n",pgm);
	fprintf(stderr,"D - invoke debugging mode\n");
	fprintf(stderr,"k - display file size in terms of GB/MB/KB\n");
	fprintf(stderr,"d - only list the dirname, not its contents\n");
	fprintf(stderr,"t - sort filenames by time\n");
	fprintf(stderr,"s - sort filenames by size\n");
	fprintf(stderr,"r - reverse sort order\n");
	fprintf(stderr,"h - produce this summary\n");
	fprintf(stderr,"R - recursively process directories\n");
	fprintf(stderr,"i - display information summary\n");
	fprintf(stderr,"a - include '.' and '..'\n");
	fprintf(stderr,"l - list in long format");
	fprintf(stderr,"n - do not display number of links");

	return;
} /* end of usage */

/*********************************************************************
*
* Function  : add_file_to_list
*
* Purpose   : Add a new entry to the list of files.
*
* Inputs    : char *filename - name of file
*             struct _stat *filestats - ptr to stat structure
*
* Output    : (none)
*
* Returns   : (nothing)
*
* Example   : add_file_to_list(filename,&filestats);
*
* Notes     : (none)
*
*********************************************************************/

void add_file_to_list(char *filename, struct _stat *filestats)
{
	debug_print("add_file_to_list(%s)\n",filename);
	if ( ++num_files > MAX_FILES ) {
		die(1,"Files maximum of %d has been exceeded\n",MAX_FILES);
	}

	files_info[num_files-1].filename = _strdup(filename);
	if ( files_info[num_files-1].filename == NULL ) {
		quit(1,"strdup failed for filename");
	}
	files_info[num_files-1].file_nlinks = filestats->st_nlink;
	files_info[num_files-1].file_mtime = filestats->st_mtime;
	files_info[num_files-1].file_size = filestats->st_size;
	files_info[num_files-1].file_mode = filestats->st_mode;

	return;
} /* end of add_file_to_list */

/*********************************************************************
*
* Function  : trim_trailing_chars
*
* Purpose   : Trim occurrences of the specified char from the end of
*             the specified buffer
*
* Inputs    : char *in_buffer - the buffer to be trimmed
*             char trim_ch - the char to be trimmed
*
* Output    : (none)
*
* Returns   : (nothing)
*
* Example   : trim_trailing_chars(buffer,'/');
*
* Notes     : (none)
*
*********************************************************************/

void trim_trailing_chars(char *in_buffer, char trim_ch)
{
	char	*ptr , ch;

	ptr = &in_buffer[strlen(in_buffer)-1];
	for ( ch = *ptr ; ch == trim_ch && ptr > in_buffer ; ch = *--ptr ) {
		*ptr = '\0';
	} /* FOR */

	return;
} /* end of trim_trailing_chars */

/*********************************************************************
*
* Function  : list_directory
*
* Purpose   : List the files under a directory.
*
* Inputs    : char *dirname - name of directory
*
* Output    : (none)
*
* Returns   : (nothing)
*
* Example   : list_directory(dirname);
*
* Notes     : (none)
*
*********************************************************************/

int list_directory(char *dirpath)
{
	_DIR	*dirptr;
	struct _dirent	*entry;
	struct _stat	filestats;
	char	*name , filename[1024] , dirname[1024];
	int		current_directory , num_subdirs , index;
	char	*subdirs[MAX_SUBDIRS];
	unsigned short	filemode;

	debug_print("list_directory(%s)\n",dirpath);

	num_subdirs = 0;

	strcpy(dirname,dirpath);
	trim_trailing_chars(dirname,'/');
	dirptr = _opendir(dirname);
	if ( dirptr == NULL ) {
		quit(1,"_opendir failed for '%s'",dirname);
	}

	current_directory = EQ(dirname,".");
	entry = _readdir(dirptr);
	for ( ; entry != NULL ; entry = _readdir(dirptr) ) {
		name = entry->d_name;
		if ( (EQ(name,"..") || EQ(name,".")) && opt_a == 0 ) {
			continue;
		}
		if ( current_directory )
			strcpy(filename,name);
		else
			sprintf(filename,"%s/%s",dirname,name);
		if ( _stat(filename,&filestats) < 0 ) {
			system_error("stat() failed for \"%s\"",filename);
		} /* IF */
		else {
			add_file_to_list(filename,&filestats);
			filemode = filestats.st_mode & _S_IFMT;
			if ( _S_ISDIR(filemode) && opt_R && NE(name,".") && NE(name,"..") ) {
				if ( ++num_subdirs > MAX_SUBDIRS ) {
					die(1,"Max number of sub-directories (%d) exceeded under '%s'\n",MAX_SUBDIRS,dirname);
				}
				subdirs[num_subdirs-1] = _strdup(filename);
				if ( subdirs[num_subdirs-1] == NULL ) {
					quit(1,"strdup failed for dir.name");
				}
			} /* IF recursive processing requested */
		} /* ELSE */
	} /* FOR */
	debug_print("list_directory(%s) ; all entries processed\n",dirname);
	_closedir(dirptr);

	if ( opt_R ) {
		for ( index = 0 ; index < num_subdirs ; ++index ) {
			list_directory(subdirs[index]);
		}
	}

	return(0);
} /* end of list_directory */

/*********************************************************************
*
* Function  : format_mode
*
* Purpose   : Format binary permission bits into a printable ASCII string
*
* Inputs    : unsigned short file_mode - mode bits from stat()
*             char *mode_bits - buffer to receive formatted info
*
* Output    : (none)
*
* Returns   : formatted mode info
*
* Example   : format_mode(filestat->st_mode,mode_info);
*
* Notes     : (none)
*
*********************************************************************/

void format_mode(unsigned short file_mode, char *mode_info)
{
	unsigned short setids;
	char *permstrs[3] , ftype , *ptr;

	setids = (file_mode & 07000) >> 9;
	permstrs[0] = perms[ (file_mode & 0700) >> 6 ];
	permstrs[1] = perms[ (file_mode & 0070) >> 3 ];
	permstrs[2] = perms[ file_mode & 0007 ];
	ftype = ftypes[ (file_mode & 0170000) >> 12 ];
	if ( setids ) {
		if ( setids & 01 ) { // sticky bit
			ptr = permstrs[2];
			if ( ptr[2] == 'x' ) {
				ptr[2] = 't';
			}
			else {
				ptr[2] = 'T';
			}
		}
		if ( setids & 04 ) { // setuid bit
			ptr = permstrs[0];
			if ( ptr[2] == 'x' ) {
				ptr[2] = 's';
			}
			else {
				ptr[2] = 'S';
			}
		}
		if ( setids & 02 ) { // setgid bit
			ptr = permstrs[1];
			if ( ptr[2] == 'x' ) {
				ptr[2] = 's';
			}
			else {
				ptr[2] = 'S';
			}
		}
	} // IF setids
	sprintf(mode_info,"%c%3.3s%3.3s%3.3s",ftype,permstrs[0],permstrs[1],permstrs[2]);

	return;
} /* end of format_mode */

/*********************************************************************
*
* Function  : display_file_info
*
* Purpose   : Display information for one file
*
* Inputs    : int index - index into list of files
*
* Output    : file information
*
* Returns   : nothing
*
* Example   : display_file_info(index);
*
* Notes     : (none)
*
*********************************************************************/

void display_file_info(int index)
{
	unsigned short	filemode;
	char	*filepath , mode_info[1024] , file_date[256] , size_buffer[100] , *suffix;
	struct tm	*filetime;
	int	bytes;
	double count;
	double double_bytes;

	filemode = files_info[index].file_mode;
	filepath = files_info[index].filename;

	if ( _S_ISDIR(filemode) ) {
		num_dirs += 1;
	}
	else {
		file_bytes += files_info[index].file_size;
	}
	if ( opt_l ) {
		format_mode(filemode,mode_info);
		filetime = localtime(&files_info[index].file_mtime);
		bytes = files_info[index].file_size;
		double_bytes = (double)bytes;
		if ( opt_k ) {
			if ( bytes >= gb ) {
				count = double_bytes / (double)gb;
				suffix = "GB";
			}
			else {
				if ( bytes >= mb ) {
					count = double_bytes / (double)mb;
					suffix = "MB";
				}
				else {
					suffix = "KB";
					count = double_bytes / (double)kb;
				}
			}
			sprintf(size_buffer,"%11.2f %s",count,suffix);
		}
		else {
			format_number_with_commas(bytes,size_buffer);
		}

		sprintf(file_date,"%3.3s %2d, %d %02d:%02d:%02d",
			months[filetime->tm_mon],
			filetime->tm_mday,1900+filetime->tm_year,filetime->tm_hour,filetime->tm_min,
			filetime->tm_sec);
		printf("%s",mode_info);
		if ( opt_n ) {
			printf(" %4d",files_info[index].file_nlinks);
		}
		printf(" %14s %s %s\n",size_buffer,file_date,filepath);
	}
	else {
		printf("%s\n",filepath);
	}

	return;
} /* end of display_file_info */

/*********************************************************************
*
* Function  : main
*
* Purpose   : program entry point
*
* Inputs    : argc - number of parameters
*             argv - list of parameters
*
* Output    : (none)
*
* Returns   : (nothing)
*
* Example   : qsort1
*
* Notes     : (none)
*
*********************************************************************/

int main(int argc, char *argv[])
{
	int		errflag , c , index , last , half;
	char	*filename , buffer[100];
	struct _stat	filestats;
	unsigned short	filemode;
	FILEDATA	first , second;
	float	count;

	errflag = 0;
	while ( (c = _getopt(argc,argv,":hgiDdtsnrRianlk")) != -1 ) {
		switch (c) {
		case 'h':
			opt_h = 1;
			break;
		case 'k':
			opt_k = 1;
			break;
		case 'a':
			opt_a = 1;
			break;
		case 'l':
			opt_l = 1;
			break;
		case 'n':
			opt_n = 1;
			break;
		case 'i':
			opt_i = 1;
			break;
		case 'r':
			opt_r = 1;
			break;
		case 'R':
			opt_R = 1;
			break;
		case 'd':
			opt_d = 1;
			break;
		case 'D':
			opt_D = 1;
			break;
		case 't':
			opt_t = 1;
			break;
		case 's':
			opt_s = 1;
			break;
		case '?':
			printf("Unknown option '%c'\n",optopt);
			errflag += 1;
			break;
		case ':':
			printf("Missing value for option '%c'\n",optopt);
			errflag += 1;
			break;
		default:
			printf("Unexpected value from getopt() '%c'\n",c);
		} /* SWITCH */
	} /* WHILE */
	if ( errflag ) {
		usage(argv[0]);
		die(1,"\nAborted due to parameter errors\n");
	} /* IF */
	if ( opt_t + opt_s > 1 ) {
		die(1,"Only one of 't' and 's' can be specified\n");
	} /* IF */
	if ( opt_h ) {
		usage(argv[0]);
		exit(0);
	} /* IF */

    /* set the INT (Ctrl-C) signal handler to 'catch_int' */
    signal(SIGINT, catch_int);
    /* signal(SIGBUS, catch_sig); */
    signal(SIGSEGV, catch_sig);

	num_args = argc - optind;
	if ( num_args <= 0 ) {
		list_directory(".");
	} /* IF */
	else {
		filename = argv[optind];
		for ( ; optind < argc ; filename = argv[++optind] ) {
			if ( _stat(filename,&filestats) < 0 ) {
				system_error("stat() failed for \"%s\"",filename);
			} /* IF */
			else {
				filemode = filestats.st_mode & _S_IFMT;
				if ( _S_ISDIR(filemode) && opt_d == 0 ) {
					list_directory(filename);
				} /* IF */
				else {
					add_file_to_list(filename,&filestats);
				} /* ELSE */
			} /* ELSE */
		} /* FOR */
	} /* ELSE */

	if ( opt_t ) { /* sort by time */
		qsort(files_info, (unsigned) num_files, sizeof(FILEDATA), compare_time);
	}
	if ( opt_s ) { /* sort by size */
		qsort(files_info, (unsigned) num_files, sizeof(FILEDATA), compare_size);
	}
	if ( opt_t + opt_s == 0 ) { /* sort by name ? */
		qsort(files_info, (unsigned) num_files, sizeof(FILEDATA), compare_name);
	}
	if ( opt_r ) { /* reverse sort order */
		last = num_files - 1;
		half = num_files / 2;
		for ( index = 0 ; index < half ; ++index , --last ) {
			first = files_info[index];
			second = files_info[last];
			files_info[index] = second;
			files_info[last] = first;
		}
	}

	for ( index = 0 ; index < num_files ; ++index ) {
		display_file_info(index);
	} /* FOR */

	if ( opt_i ) {
		printf("\n");
		printf("Number of directories = %d\n",num_dirs);
		format_number_with_commas(file_bytes,buffer);
		printf("Number of files = %d , files size total = %s bytes",num_files,buffer);
		if ( file_bytes >= gb ) {
			count = (float)file_bytes / (float) gb;
			printf("  %.2f GB",count);
		}
		else {
			if ( file_bytes >= mb ) {
				count = (float)file_bytes / (float) mb;
				printf("  %.2f MB",count);
			}
			else {
				if ( file_bytes >= kb ) {
					count = (float)file_bytes / (float) kb;
					printf("  %.2f KB",count);
				}
			}
		}
		printf("\n");
	}

	exit(0);
} /* end of main */
