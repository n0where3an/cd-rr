/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef __RR_H__
#define __RR_H__

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __WIN32__
#include "getopt.h"
#else
	#include <getopt.h>
#endif /*__WIN32__*/
//#include "rrmath.h"
//#include "rrfile.h"
#include "rrlist.h"

#define INBUFLEN ( 4 * _MAX_PATH + 64 )

#define RR_RC_OK            0
#define RR_RC_INCOMPLETE    1
#define RR_RC_WARNINGS      1
#define RR_RC_UNUSABLE      2
#define RR_RC_FATAL         3
#define RR_RC_ILLEGAL_PARAM 4

#define M2CDM_CMD_VOLUME	/*"-volume"*/		"-v"
#define M2CDM_CMD_OUTPUT	/*"-output"*/		"-o"
#define M2CDM_CMD_FILE		/*"-file"*/			"-f"
#define M2CDM_CMD_MOVIE		/*"-movie"*/		"-m"
#define M2CDM_CMD_DIR			/*"-dir"*/			"-d"
#define M2CDM_CMD_EXT			/*"-ext"*/			"-e"
#define M2CDM_CMD_DOS			/*"-dos"*/			"-c"
#define M2CDM_CMD_EXTNAME	/*"-extname"*/	"-x"
#define M2CDM_CMD_SINGLE	/*"-single"*/		"-s"
#define M2CDM_CMD_ISO1		"-isolevel1"
#define M2CDM_CMD_KEEPEXT	/*"-extname"*/	"-x"

//#define OGM_LEN 2324
#define RR_STREAMNUM 6
#define ISO_LEN 2048
#define OGM_LEN global_opts.ogm_len
#define MAX_REDUNDANCY ( 10000 * RR_STREAMNUM )
#define MAX_OPTS_LEN 0x1000

#define FULL_READINESS 1000
#define LONGBLANKLINE \
	"\r                                                                               \r"

#define FOPEN_TEXT "t"
#define FOPEN_BINARY "b"

#define RR_M2CDM_EXT ".um2"
#define RR_DEFAULT_XCD_EXT "xcd"

#define NO_ERROR			0
#define SOFT_ERROR	(-2)
#define HARD_ERROR	(-3)
#define FATAL_ERROR	(-4)

typedef enum {
	VERBOSE_QUIET = 0,
	VERBOSE_NORMAL = 1,
	VERBOSE_INFO = 2,
	VERBOSE_DEBUG = 3
} verbose_t;
#define CLEAR_LINE 0x1000	/* флаг "чистить строку перед выводом" */

//#include "rrfile.h"
typedef enum {
	PROGRAM_RRENC = 0x434E45,	// 'ENC'
	PROGRAM_RRDEC = 0x434544	// 'DEC'
} program_t;
typedef struct {
	program_t program;
	char *filelist;
	char *src_base;
	char *tgt_base;
#ifdef RR_UPDATE_DIR
	char *upd_base;
	int upd_protected;
#endif /* RR_UPDATE_DIR */
	char *xcd_ext;
	char *out_base;
	char *volume;
	char *rr_header;
	char *plain_listfile;
//	int verbose_level;
	verbose_t verbose_level;
	int ogm_len;
	int errcount;
	int version[ 2 ];
	int all_files;
	int overwrite;
#ifdef RR_NO_RESTORE
	int no_restore;
#endif /* RR_NO_RESTORE */
	int no_readfiles;
	int fake_output;
	int strict_mode;
	int xcd_output;
	int multitrack;
//	rrchain_t *all;
//	char m2cdm_opts[ MAX_OPTS_LEN ];
	int old_compat_mode;
	int zero_as_bad;
#ifdef RR_DMG_LIST
	char *damage_list;
#endif /* RR_DMG_LIST */
#ifdef RR_1PASS_COUNT
	int max_for_1pass;
#endif /* RR_1PASS_COUNT */
#ifdef RR_KEEP_BAD
	int keep_bad_files;
#endif /* RR_KEEP_BAD */
#ifdef RR_KEEP_EXISTING
	int keep_exist_files;
#endif /* RR_KEEP_EXISTING */
	int check;
#ifndef RR_CHECK_ALWAYS
	int checkonly;
#endif /* RR_CHECK_ALWAYS */
#ifdef RR_COPY_ONLY
	int copy_only;
#endif /* RR_COPY_ONLY */
#ifdef RR_NO_INIT_READ
	int init_noread;
#endif /* RR_NO_INIT_READ */
#ifdef RR_IGNORE_DIR
	rrlist_mode_t process_dir;
#endif /* RR_IGNORE_DIR */
#ifdef RR_BARE_OUTPUT
	int bare_status;
#endif /* RR_BARE_OUTPUT */
#ifdef RR_OEM_API
	int ansi_api;
#endif /* RR_OEM_API */
#ifdef RR_STORE_EDC
	char *keep_digest;
#endif /* RR_STORE_EDC */
#ifdef RR_KEEP_OLD_EXT
	int keep_ext;
#endif /* RR_KEEP_OLD_EXT */
#ifdef RR_OUTPUT_REDIR
	int out_redirected;
#endif /* RR_OUTPUT_REDIR */
} opts_t;

extern opts_t global_opts;

extern const char *progname;
struct rrchain_s; 
typedef struct rrchain_s rrchain_t;

//void usage( void );
int rr_parse_args( int argc, char * const *argv, 
		const struct option *long_opts );
int rr_makedestdir( const char *fullname );
int rrc_fgets( FILE *file, char *name, int *line, char *buf, int buflen );
void usage( void );
int rr_ferror( FILE *file, char *fname, char *comment );
void rr_printstatus( int readiness, time_t start_time, int errors,
#ifdef RR_SHOW_LOST
		int lost,
#endif /* RR_SHOW_LOST */
		char *suffix );
int rr_printmsg( verbose_t verbose, char *fmt, ... );
int rr_make_seclen( int type );
#ifdef RR_NEW_ENC
#if 0 /* и на фига я это впихнул ? */
int rr_readchain( rrchain_t *rrchain, char *buf,
		int bufblks, int max_dose, rrtable_t *rr, int *error_count );
int rr_check_result( rrchain_t *rrchain, rrtable_t *rr, char *buf,
		char **errlocs, rritem_t *decoy );
#endif /*0*/
#endif /* RR_NEW_ENC */

// для отладки:
#ifdef MALLOC_DEBUG
#error "malloc_debug"
void *rr_guard_malloc( size_t size );
void rr_guard_free( void *ptr );

#define malloc rr_guard_malloc
#define free rr_guard_free
#endif /* MALLOC_DEBUG */

#ifndef RR_BASE_EXT
int rr_test_outbase( void );
#endif /* RR_BASE_EXT */

#ifndef RR_LIST_2PASS
	#define SPECIAL_FILE_BUFFERS( name, desc ) \
		char name ## _buf[ _MAX_PATH ], name ## _buf2[ _MAX_PATH ], \
				*name ## _desc = (desc), \
				name ## _fullname[ _MAX_PATH ], *name ## _path
	#define CONCAT_NAME( name, part ) \
		( name ## _ ## part )
#else /* RR_LIST_2PASS */
	typedef struct {
		char *_desc;
		FILE *_file;
		char
			_buf[ _MAX_PATH ],
			_buf2[ _MAX_PATH ],
			_fullname[ _MAX_PATH ],
			*_fname, *_path;
	} spec_file_t;
	#define SPECIAL_FILE_BUFFERS( name, desc ) \
		spec_file_t name ## _struct = { (desc) }; \
		spec_file_t *name ## _spec = &(name ## _struct)

	#define CONCAT_NAME( name, part ) \
		( (name ## _spec)->_ ## part )
#endif /* RR_LIST_2PASS */
#define M2CDM_FILE_BUFFERS  \
				SPECIAL_FILE_BUFFERS( m2cdm, "mode2cdmaker paramfile" )
#define PLAIN_FILE_BUFFERS  \
				SPECIAL_FILE_BUFFERS( plain, "plain list file" )
#define RRLIST_FILE_BUFFERS( desc ) \
				SPECIAL_FILE_BUFFERS( rrlist, desc )
#define RRDATA_FILE_BUFFERS \
				SPECIAL_FILE_BUFFERS( rrdata, "RR-data file" )
#define SRC_FILE_BUFFERS( desc )    \
				SPECIAL_FILE_BUFFERS( src, desc )

#define M2CDM_PATH_FMT M2CDM_CMD_OUTPUT "\n%s\n"
#define M2CDM_PATH_VAR , CONCAT_NAME( m2cdm, path )
#ifndef RR_OEM_API
	#define OEM_ANSI_CHARSET "-c\n"
#else /* RR_OEM_API */
	#define OEM_ANSI_CHARSET ( global_opts.ansi_api ? "" : M2CDM_CMD_DOS "\n" )
//	#define M2CDM_PATH_FMT
//	#define M2CDM_PATH_VAR
#endif /* RR_OEM_API */

#ifndef RR_CHECK_ISO1
	#define SET_ISO1 M2CDM_CMD_ISO1 "\n"
	#define CHECK_ISO1 do {} while (0)
#else /* RR_CHECK_ISO1 */
	#define SET_ISO1 /* "-isolevel1\n" */
	#define CHECK_ISO1 \
		do { \
			if ( !rr_check_iso1( rest ) ) \
				is_iso1 = 0; \
		} while (0)
	int rr_check_iso1( char *fullname );
#endif /* RR_CHECK_ISO1 */

#ifdef __GCC__
#define M2CDM_VOLUMELABEL \
	do { \
		if ( global_opts.volume != NULL ) \
			/* fprintf( m2cdm_file, M2CDM_CMD_VOLUME "\n%s\n", global_opts.volume ); */ \
			PUT_M2CDM_LINE( M2CDM_CMD_VOLUME "\n%s\n", global_opts.volume ); \
	} while (0)
#else
#define M2CDM_VOLUMELABEL \
	do { \
		if ( global_opts.volume != NULL ) \
			/* fprintf( m2cdm_file, M2CDM_CMD_VOLUME "\n%s\n", global_opts.volume ); */ \
			PUT_M2CDM_LINE( *"*/ M2CDM_CMD_VOLUME "\n%s\n", global_opts.volume /*"* ); \
	} while (0)
#endif
#ifndef RR_LIST_2PASS
	#define M2CDM_VOLUMELABEL_ATFIRST M2CDM_VOLUMELABEL
	#define M2CDM_FILE m2cdm_file
#else /* RR_LIST_2PASS */
	#define M2CDM_VOLUMELABEL_ATFIRST do {} while (0)
	#define M2CDM_FILE m2cdm_spec->_file
#endif /* RR_LIST_2PASS */

#ifndef RR_KEEP_OLD_EXT
	#define M2CDM_OLDEXT_FMT
	#define M2CDM_OLDEXT_VAR
#else /* RR_KEEP_OLD_EXT */
	#define M2CDM_OLDEXT_FMT \
		"%s"
	#define M2CDM_OLDEXT_VAR \
		, ( global_opts.keep_ext ) ? M2CDM_CMD_KEEPEXT "\n" : ""
#endif /* RR_KEEP_OLD_EXT */

//#ifndef RR_CUSTOM_XCD_EXT
	#define M2CDM_XCDEXT_FMT M2CDM_CMD_EXT "\n%s\n"
	#define M2CDM_XCDEXT_VAR , global_opts.xcd_ext
//#else /* RR_CUSTOM_XCD_EXT */
//	#define M2CDM_XCDEXT_FMT
//	#define M2CDM_XCDEXT_VAR
//#endif /* RR_CUSTOM_XCD_EXT */

#ifdef __GCC__
#define PUT_SPECIAL_LINE( name, ... ) \
	do { \
		if ( CONCAT_NAME( name, file ) != NULL ) { \
			fprintf( CONCAT_NAME( name, file ), __VA_ARGS__ ); \
			CHECK_SPECIAL_FILE( name ); \
		} \
	} while (0)

#else
#define PUT_SPECIAL_LINE( name, line ) \
	do { \
		if ( CONCAT_NAME( name, file ) != NULL ) { \
			fprintf( CONCAT_NAME( name, file ), /line/ ); \
			CHECK_SPECIAL_FILE( name ); \
		} \
	} while (0)
#endif

#ifdef __GCC__
#define PUT_M2CDM_LINE( ... ) \
	PUT_SPECIAL_LINE( m2cdm,__VA_ARGS__ )
#else
#define PUT_M2CDM_LINE( line ) \
	PUT_SPECIAL_LINE( m2cdm, line )
#endif

#ifdef __GCC__
#define CREATE_M2CDM_FILE \
	do { \
		CREATE_SPECIAL_FILE( m2cdm, RR_M2CDM_EXT, NULL ); \
		PUT_M2CDM_LINE( "%s" SET_ISO1 "%s" M2CDM_PATH_FMT M2CDM_OLDEXT_FMT \
				M2CDM_XCDEXT_FMT, \
				global_opts.multitrack ? "" : M2CDM_CMD_SINGLE "\n", \
				OEM_ANSI_CHARSET M2CDM_PATH_VAR M2CDM_OLDEXT_VAR \
				M2CDM_XCDEXT_VAR ); \
		M2CDM_VOLUMELABEL_ATFIRST; \
		/*}*/ \
	} while (0)
#else
#define CREATE_M2CDM_FILE \
	do { \
		CREATE_SPECIAL_FILE( m2cdm, RR_M2CDM_EXT, NULL ); \
		PUT_M2CDM_LINE( *"*/ "%s" SET_ISO1 "%s" M2CDM_PATH_FMT M2CDM_OLDEXT_FMT \
				M2CDM_XCDEXT_FMT, \
				global_opts.multitrack ? "" : M2CDM_CMD_SINGLE "\n", \
				OEM_ANSI_CHARSET M2CDM_PATH_VAR M2CDM_OLDEXT_VAR \
				M2CDM_XCDEXT_VAR /*"* ); \
		M2CDM_VOLUMELABEL_ATFIRST; \
		/*}*/ \
	} while (0)
#endif


#define CREATE_PLAIN_FILE \
	do { \
		CREATE_SPECIAL_FILE( plain, NULL, NULL ); \
	} while (0)
#define MAKE_NAME( name, ext, base ) \
	do { \
		if ( ( CONCAT_NAME( name, path )= \
				rr_fullname( CONCAT_NAME( name, buf ), /*global_opts.src_*/base, \
				CONCAT_NAME( name, fname ), NULL, CONCAT_NAME( name, desc ), \
				1 ) ) == NULL ) \
			goto err01; \
		_makepath( CONCAT_NAME( name, fullname ), NULL, NULL, \
				CONCAT_NAME( name, path ), ext ); \
		/* CONCAT_NAME( name, fname ) = CONCAT_NAME( name, buf2 ); */ \
	} while (0)
#define MAKE_DIR( name ) \
	do { \
		if ( rr_makedestdir( CONCAT_NAME( name, fullname ) ) != 0 ) { \
			rr_printmsg( VERBOSE_QUIET, "%s %s: error creating dir\n", \
					CONCAT_NAME( name, desc ), CONCAT_NAME( name, fullname ) ); \
			goto err01; \
		} \
	} while (0)
#define OPEN_FILE( name, mode ) \
	do { \
		if ( ( CONCAT_NAME( name, file ) = \
				fopen( CONCAT_NAME( name, fullname ), mode ) ) == NULL ) { \
			rc = errno; \
			rr_printmsg( VERBOSE_QUIET, \
					"%s %s: open" /*"/create"*/" failed: %d - %s\n", \
					CONCAT_NAME( name, desc ), CONCAT_NAME( name, fname ), \
					rc, strerror( rc ) ); \
			goto err01; \
		} \
	} while (0)
#define MAKE_SHORT_NAME( name, ext ) \
	do { \
		_makepath( CONCAT_NAME( name, buf2 ), NULL, NULL, \
			CONCAT_NAME( name, fname ), ext ); \
		CONCAT_NAME( name, fname ) = CONCAT_NAME( name, buf2 ); \
	} while (0)

#define OPEN_SPECIAL_FILE( name, ext, base ) \
	do { \
		if ( CONCAT_NAME( name, fname ) != NULL ) { \
			MAKE_NAME( name, ext, base ); \
			OPEN_FILE( name, "r" FOPEN_TEXT ); \
			MAKE_SHORT_NAME( name, ext ); \
		} \
	} while (0)
#define CLOSE_SPECIAL_FILE( name ) \
	do { \
		int spec_rc = rr_ferror( CONCAT_NAME( name, file ); \
		fclose( CONCAT_NAME( name, file ) ); \
		if ( spec_rc < NO_ERROR ) \
			goto err01; \
	} while (0)

#define CREATE_SPECIAL_FILE( name, ext, base ) \
	do { \
		if ( CONCAT_NAME( name, fname ) != NULL ) { \
			MAKE_NAME( name, ext, base ); \
			MAKE_DIR( name ); \
			OPEN_FILE( name, "w" FOPEN_BINARY /*TEXT*/ ); \
			MAKE_SHORT_NAME( name, ext ); \
		} \
	} while (0)

#define CHECK_SPECIAL_FILE( name ) \
	do { \
		if ( rr_ferror( CONCAT_NAME( name, file ), \
				CONCAT_NAME( name, fname ), CONCAT_NAME( name, desc ) ) < NO_ERROR ) \
			goto err01; \
	} while (0)

#define RRDATA_NAME_FILE( base ) \
	do { \
		if ( rrdata_fname != NULL ) { \
			MAKE_NAME( rrdata, NULL, base ); \
			MAKE_DIR( rrdata ); \
			MAKE_SHORT_NAME( rrdata, NULL ); \
		} \
	} while (0)

#define STRINGIFY( x ) \
	"" #x ""

#ifndef RR_CHECK_ALWAYS
	#define CHECK_ONLY global_opts.checkonly
	#define EXTRA_CHECK_ONLY global_opts.checkonly
#else /* RR_CHECK_ALWAYS */
	#define CHECK_ONLY ( global_opts.tgt_base == NULL )
	#define EXTRA_CHECK_ONLY ( global_opts.check && CHECK_ONLY )
#endif /* RR_CHECK_ALWAYS */
#ifdef RR_NO_RESTORE
	#define LOOK_ONLY ( global_opts.no_restore && CHECK_ONLY )
#else /* RR_NO_RESTORE */
	#define LOOK_ONLY 0
#endif /* RR_NO_RESTORE */
#if defined ( RR_OEM_API ) && defined( __WIN32__)
	#define ANSI2OEM( string ) \
		do { \
			if ( (string) != NULL ) \
				CharToOem( (string), (string) ); \
		} while (0)
#endif /* RR_OEM_API && __WIN32__*/

#ifdef RR_DEC_PATH
	#ifdef RR_IGNORE_DIR
char *rr_combo_name( char *outname, const char *base, const char *name );
	#endif /* RR_IGNORE_DIR */
#endif /* RR_DEC_PATH */

#ifdef RR_LEGALZERO
	#define rr_atoi( str ) atoi( str )
#else /* RR_LEGALZERO */
int rr_atoi( const char *inbuf );
#endif /* RR_LEGALZERO */
/* время модификации из rr-list'а: time_t, без обрезания до int (2038) */
#define rr_atotime( str ) ( (time_t) strtoll( (str), NULL, 10 ) )

#ifndef __WIN32__
void  _splitpath(const char* path, char* drive,  char* dir,  char* fname,  char* ext );
void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext);
#endif /*__WIN32__*/

#endif /* __RR_H__ */
