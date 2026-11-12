/*

    2012/11/02 - 1.5.1e - Elicit Gingers

    defaults.h: use date of compilation instead of predefined one

    2007/08/04 - 1.5.1d - Elicit Gingers

    defaults.h: preserve original timestamps of files placed on disc

*/

#ifndef __DEFAULTS_H__
#define __DEFAULTS_H__

// for properly timestamp file -- by EG:
#include <time.h>

/* Compatibility stuff */

#ifdef _WIN32
#if defined __BORLANDC__ || defined _MSC_VER
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif
#endif

/* Strings */

extern char FORM2_EXT[MAX_PATH];

#define APP_VERSION "1.5.1d"
//#define APP_DATE "2007/08/04"

/* Some default settings for the CD */

#define CD_SYSTEM_ID "CD-RTOS CD-BRIDGE"  /* mandatory */
extern char CD_VOLUME_ID[33];   

#define CD_VOLUME_SET_ID  " "
#define CD_PUBLISHER_ID   " "
#define CD_PREPARER_ID    " "
#define CD_APPLICATION_ID " "

/* Extern variables */

extern int ISO_FS_BLOCKS;
extern int START_FILE_EXTENT;

/* Templates and constants */

#define LEN2BLOCKS(x) (((x)+2047)>>11)

#define PREGAP_SIZE 150

#define MAX_FILES 4096
#define MAX_DIRS  512

#ifdef _WIN32
#define DIR_SLASH '\\'
#else
#define DIR_SLASH '/'
#endif

#define PASS_1 1
#define PASS_2 2
#define PASS_3 3

/* Structures */

enum { DIR, FORM1, FORM2, NOTFILE };
enum { BIN, CCD, RAW };

typedef struct entry_s {
        char *name;
        int type;   // DIR, FORM1, FORM2, NOTFILE
        int size;   // in blocks
        int length; // in bytes
        int extent; // start block
        int number; // form2 file number, or directory id
        int parent; // parent directory id
				struct tm ft; // file last modification time -- by EG
        } entry_s;

typedef struct dir_s {
        char *record;
        char *name;
        int len;    // in bytes
        int extent; // start block
        int parent; // parent dir block
        int parent_num;
        } dir_s;

typedef struct opts_s {
        int long_file_names;
        int ascii;
        int dos_charset;
        int form2_ext_to_name;
        int single_track;
        int imgformat; // BIN, CCD or RAW
        } opts_s;

#endif /* __DEFAULTS_H__ */

