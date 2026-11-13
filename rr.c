/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef __GNUC__
#include <direct.h>
#endif /* __GNUC__ */
#include <stdarg.h>
#include "rr.h"
#ifdef __WIN32__
#include "getopt.h"
#else
	#include <libgen.h>
	#include <getopt.h>
#endif /*__WIN32__*/
//#include "rrmath.h"
//#include "rrfile.h"
#include "rrfile.h"
#include "rrlist.h"

#ifndef RR_TEST_ONLY
#define MAX_OPTS 63

#ifndef RR_NOARGS
#define CHECK_VERSION( ver ) \
	do { \
		if ( ( (ver) < 0 ) || ( (ver) > 1 ) ) \
			goto ver_error; \
		global_opts.version[ ver ] = 1; \
	} while (0)

//#define CHECK_VERSION( ver )

#define STR_OPTARG( name ) \
	do { \
		if ( optarg == NULL ) \
			goto lack_arg; \
		global_opts.name = optarg; \
	} while (0)
#ifndef RR_LEGALZERO
	#define INT_OPTARG( name /*, min, max */ ) \
		do { \
			size_t jjx; \
			if ( optarg == NULL ) \
				goto lack_arg; \
			for ( jjx = 0; jjx < strlen( optarg ); jjx++ ) \
				if ( !isdigit( optarg[ jjx ] ) ) \
					goto wrong_arg; \
			global_opts.name = atoi( optarg ); \
			/* if ( ( (min) <= (max) ) && ( */ \
				/* ( global_opts.name < (min) ) || ( global_opts.name > (max) )*/ \
					/* ) ) */ \
				/* goto wrong_arg; */ \
		} while (0)
#else /* RR_LEGALZERO */
	#define INT_OPTARG( name /*, min, max */ ) \
		do { \
			if ( optarg == NULL ) \
				goto lack_arg; \
			if ( ( global_opts.name = atoi( optarg ) ) < -1 ) \
				goto wrong_arg; \
		} while (0)
#endif /* RR_LEGALZERO */

int rr_parse_args( int argc, char * const *argv, 
const struct option *long_opts ) {
	char short_opts[ MAX_OPTS + 1 ] = "+";
	int jx, jy, next_option;
//#ifdef RR_CHECK_ALWAYS
//	global_opts.check = 1;
//#endif /* RR_CHECK_ALWAYS	*/
#ifdef RR_OUTPUT_REDIR
	struct _stat stat_buf;
	global_opts.out_redirected = 
		!( ( _fstat( _fileno( stdout ), &stat_buf ) == 0 )
			&& ( stat_buf.st_mode == _S_IFCHR ) );
#endif /* RR_OUTPUT_REDIR */
	global_opts.process_dir = DIR_EXTRACT;	// по умолчанию
	for ( jx = 0, jy = strlen( short_opts ); 
		( ( jy < MAX_OPTS - 2 ) && ( long_opts[ jx ].name != NULL ) ); jx++ ) {
		short_opts[ jy++ ] = (char) long_opts[ jx ].val;
		if ( long_opts[ jx ].has_arg ) {	// нужен аргумент
			short_opts[ jy++ ] = ':';
			if ( long_opts[ jx ].has_arg > 1 ) {	// необязательный
				short_opts[ jy++ ] = ':';
			}
		}
		short_opts[ jy ] = 0;
	}

	while ( ( next_option = getopt_long( argc, argv, short_opts, long_opts,
					NULL ) ) != -1 ) {
		switch ( next_option ) {
			case 'h': {
					usage();
					exit( 0 );
				} break;
			case 'a': {
#ifndef RR_OEM_API
					CHECK_VERSION( 1 );
					global_opts.all_files = 1;
#else /* RR_OEM_API */
					switch ( global_opts.program ) {
						case PROGRAM_RRDEC: {
								CHECK_VERSION( 1 );
								global_opts.all_files = 1;
							}	break;
						case PROGRAM_RRENC: {
								global_opts.ansi_api = 1;
							}	break;
					}
#endif /* RR_OEM_API */
				} break;
			case 'w': {
					global_opts.overwrite = 1;
				} break;
			case 'x': {
					switch ( global_opts.program ) {
#ifdef RR_KEEP_OLD_EXT
						case PROGRAM_RRENC: {
								global_opts.keep_ext = 1;
							}	break;
#endif /* RR_KEEP_OLD_EXT */
						case PROGRAM_RRDEC: {
								CHECK_VERSION( 1 );
								global_opts.xcd_output = 1;
							}	break;
					}
				} break;
			case 'p': {
					switch ( global_opts.program ) {
						case PROGRAM_RRENC: {
								STR_OPTARG(	plain_listfile );
							}	break;
#ifdef RR_COPY_ONLY
						case PROGRAM_RRDEC: {
								global_opts.copy_only = 1;
							}	break;
#endif /* RR_COPY_ONLY */
					}
				} break;
			case 'n': {
					switch ( global_opts.program ) {
#ifdef RR_NO_RESTORE
						case PROGRAM_RRDEC: {
								global_opts.no_restore = 1;
							}	break;
#endif /* RR_NO_RESTORE */
						case PROGRAM_RRENC: {
								global_opts.fake_output = 1;
								global_opts.no_readfiles = 1;
							}	break;
						default:
							break;
					}
				} break;
			case 'k': {
//				CHECK_VERSION( 1 );
#ifndef RR_KEEP_EXISTING
					global_opts.fake_output = 1;
#else /* RR_KEEP_EXISTING */
					switch ( global_opts.program ) {
						case PROGRAM_RRDEC: {
								global_opts.keep_exist_files = 1;
							}	break;
						case PROGRAM_RRENC: {
								global_opts.fake_output = 1;
							}	break;
					}
#endif /* RR_KEEP_EXISTING */
				} break;
#ifdef RR_KEEP_BAD
			case 'b': {
					global_opts.keep_bad_files = 1;
				} break;
#endif /* RR_KEEP_BAD */
			case 'q': {
					if ( --global_opts.verbose_level < VERBOSE_QUIET )
						global_opts.verbose_level = VERBOSE_QUIET;
				} break;
			case 'v': {
					if ( ++global_opts.verbose_level > VERBOSE_DEBUG )
						global_opts.verbose_level = VERBOSE_DEBUG;
				} break;
			case '@': {
					CHECK_VERSION( 1 );
					STR_OPTARG(	filelist );
				} break;
			case 'o': {
					CHECK_VERSION( 1 );
					STR_OPTARG( out_base );
				} break;
			case 's': {
					CHECK_VERSION( 1 );
					STR_OPTARG( src_base );
				} break;
			case 't': {
					CHECK_VERSION( 1 );
					STR_OPTARG( tgt_base );
				} break;
#ifdef RR_UPDATE_DIR
			case 'u': {
					switch ( global_opts.program ) {
						case PROGRAM_RRENC: {
								global_opts.multitrack = 1;
							}	break;
						case PROGRAM_RRDEC: {
								CHECK_VERSION( 1 );
								STR_OPTARG( upd_base );
							}	break;
					}
				} break;
#endif /* RR_UPDATE_DIR */
			case 'e': {
					CHECK_VERSION( 1 );
					STR_OPTARG( xcd_ext );
				} break;
			case 'l': {
					CHECK_VERSION( 1 );
					STR_OPTARG( volume );
				} break;
#ifdef RR_1PASS_COUNT
			case 'f': {
//				CHECK_VERSION( 1 );
//				if ( optarg != NULL )
					INT_OPTARG( max_for_1pass );
					global_opts.max_for_1pass =
						( global_opts.max_for_1pass + RR_STREAMNUM - 1 ) / RR_STREAMNUM;
				} break;
#endif /* RR_1PASS_COUNT */
#ifndef RR_CHECK_ALWAYS
			case 'c': {
//				CHECK_VERSION( 1 );
					global_opts.check = 1;
				} break;
#endif /* RR_CHECK_ALWAYS	*/
			case 'r': {
					switch ( global_opts.program ) {
						case PROGRAM_RRENC: {
//						CHECK_VERSION( 1 );
		//				INT_OPTARG( errcount, 0, 16384 );
								STR_OPTARG( rr_header );
							}	break;
						case PROGRAM_RRDEC: {
								CHECK_VERSION( 1 );
								global_opts.upd_protected = 1;
							}	break;
					}
				} break;
			case 'i': {
					switch ( global_opts.program ) {
						case PROGRAM_RRDEC: {
								CHECK_VERSION( 0 );
								global_opts.ogm_len = ISO_LEN;
							}	break;
						case PROGRAM_RRENC: {
								global_opts.strict_mode = 1;
							}	break;
					}
				} break;
//			case 'x': {
//				CHECK_VERSION( 0 );
//				global_opts.ogm_len = XCD_DATA_LEN;
//			} break;
			case 'z': {
					global_opts.zero_as_bad = 1;
				} break;
#ifdef RR_NO_INIT_READ
			case 'c': {
					global_opts.init_noread = 1;
				} break;
#endif /* RR_NO_INIT_READ */
#ifdef RR_IGNORE_DIR
			case 'd': {
					global_opts.process_dir = DIR_EXPLICIT;
				} break;
#endif /* RR_IGNORE_DIR */
#ifdef RR_BARE_OUTPUT
			case 'g': {
					global_opts.bare_status = 1;
				} break;
#endif /* RR_BARE_OUTPUT */
#ifdef RR_STORE_EDC
	#ifdef RR_BARE_OUTPUT
		#error "RR_STORE_EDC and RR_BARE_OUTPUT share the same switch letter!"
	#endif /* RR_BARE_OUTPUT */
			case 'g': {
					STR_OPTARG( keep_digest );
				} break;
#endif /* RR_STORE_EDC */
			default: {
//				printf( "Illegal option!\n", next_option );
//		usage();
				common_error:
//				copyright();
					printf( "Use -h to help\n" );
					exit( 1 );
				} break;
		}
	}

	return ( optind );
ver_error:
	printf( "Mixed options of different versions!\n" );
	goto common_error;
lack_arg:
	printf( "Absent parameter of option \"-%c\"!\n", next_option );
	goto common_error;
wrong_arg:
	printf( "Wrong parameter of option \"-%c\"!\n", next_option );
	goto common_error;
}
#ifndef __WIN32__
void  _splitpath(const char* path, char* drive,  char* dir,  char* fname,  char* ext ) {
	// POSIX-замена _splitpath() из MSVCRT. Отличие: ext - без ведущей точки.
	char tmppath[_MAX_PATH];
	const char *base, *dot;
	size_t li;
	if (drive) {
		drive[0]=0;}
	if (dir) {
		strncpy(tmppath, path, _MAX_PATH - 1); tmppath[_MAX_PATH - 1] = 0;
		strcpy(dir,dirname (tmppath));
		li= strlen(dir);
		if (li && *(dir + li - 1) != '/') //47
			strcat(dir, "/");
	}
	base = strrchr(path, '/');
	base = ( base == NULL ) ? path : base + 1;
	dot = strrchr(base, '.');
	if (fname) {	// имя без каталога и без расширения
		li = ( dot == NULL ) ? strlen(base) : (size_t) ( dot - base );
		memcpy(fname, base, li);
		fname[li] = 0;
	}
	if (ext) {
		strcpy(ext, ( dot == NULL ) ? "" : dot + 1);
	}
}

void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext){
	int dir_len;

	if (dir != NULL) {
		strcpy(path, dir);
		dir_len = strlen(dir);
		if (dir_len && *(dir + dir_len - 1) != '/') //47
			strcat(path, "/");
	}
	else
		(*path)=0;

	if (fname != NULL && strlen(fname)) {
		strcat(path, fname);
		if (ext != NULL && strlen(ext)) {
			if (*ext != '.')
				strcat(path, ".");
			strcat(path, ext);
		}
	}
}
#endif /*__WIN32__*/

static int makeabsdir( char *path, char *drive, char *tmppath,
struct stat *dummy ) {
	struct _stat l_stat;
	int rc;
	char 
	fname[_MAX_FNAME], ext[_MAX_EXT], dir[_MAX_DIR];
	char *dirend = path + strlen( path ) - 1;
#ifdef __WIN32__	
	if ( *dirend != '\\' ) {
#else		
		if ( *dirend != '/' ) {
#endif /*__WIN32__*/
			rr_printmsg( VERBOSE_QUIET,
				"target dir %s: has illegal character at end\n", path );
			return ( -1 );
		}
		if ( dirend > path ) *dirend = 0;
		_splitpath( path, NULL, dir, fname, ext );
	makedir:
//#error "Разобраться, что делать, если fullname == \"nul\""
		_makepath( tmppath, drive, dir, fname, ext );
#ifdef __WIN32__			
// %%% Microsoft specific: %%%
		if ( strcmp( tmppath, "\\\\." ) == 0 ) {	// скорее всего имя девайса
			rr_printmsg( VERBOSE_QUIET,
				"target dir %s: is a system-internal object\n", tmppath );
			return ( 0 );
		}
#endif /*__WIN32__*/		
		if ( _stat( tmppath, &l_stat ) == 0 ) {
			if ( ( l_stat.st_mode & _S_IFMT ) == _S_IFDIR ) {
				return ( 0 );
			}
			rr_printmsg( VERBOSE_QUIET,
				"target dir %s: is a non-directory\n", tmppath );
			return ( EEXIST );
		}
#ifdef __WIN32__ /*__WIN32__*/
		if ( ( rc = _mkdir( tmppath ) ) != 0 ) {
#else
			if ( ( rc = mkdir( tmppath,0777 ) ) != 0 ) {
#endif /*__WIN32__*/
				if ( ( rc = errno ) == ENOENT ) {
					if ( ( rc = makeabsdir( dir, drive, tmppath, dummy ) ) != 0 ) {	// не удалось создать
						return ( rc );
					}
					goto makedir;
				} else if ( rc == EEXIST ) {
					return ( 0 );
				} else {
					rc = errno;
					rr_printmsg( VERBOSE_QUIET, "target dir %s: create error: %d - %s\n",
						tmppath, rc, strerror( rc ) );
				}
			}
			return ( rc );
		}
int rr_makedestdir( const char *fullname ) {
	char drive[_MAX_DRIVE], dir[_MAX_DIR], tmppath[_MAX_PATH];
//	char *pathtop;
	struct stat dummy;
	int rc;
	if ( strlen( fullname ) >= _MAX_PATH ) {
		rr_printmsg( VERBOSE_QUIET,
			"target dir %s: illegal full path\n", fullname );
		return -1;
	}
	strcpy( tmppath, fullname );
	_splitpath( tmppath, drive, dir, NULL, NULL );
	_makepath( tmppath, drive, dir, NULL, NULL );
	rc = makeabsdir( dir, drive, tmppath, &dummy );
	errno = 0;
	return ( rc );
}
#endif /* RR_NOARGS */

//#define INBUFLEN ( _MAX_PATH + 64 )
char inbuf[ INBUFLEN ];

int rrc_fgets( FILE *file, char *name, int *line, char *buf, int buflen ) {
	int rc;
	(*line)++; 
	if ( fgets( buf, buflen, file ) == NULL ) {
//notread:
		if ( feof( file ) ) {
			return ( 0 );
//			goto eof;
		}
		rc = ferror( file );
#ifndef RR_NOARGS
		rr_printmsg( VERBOSE_QUIET,
#else /* RR_NOARGS */
			fprintf( stderr,
#endif /* RR_NOARGS */
				"source filelist %s, line %d: fgets failed: %d - %s\n", 
				name, *line, rc, strerror( rc ) );
		return ( -rc );
//		goto error;
	}
	rc = strlen( buf );
	if ( buf[ rc - 1 ] != '\n' ) {
#ifndef RR_NOARGS
		rr_printmsg( VERBOSE_QUIET,
#else /* RR_NOARGS */
			fprintf( stderr,
#endif /* RR_NOARGS */
				"source filelist %s, line %d: is too long\n", name, *line );
		return ( -1 );
	}
	buf[ rc - 1 ] = 0;
	return ( rc );
}

#ifndef RR_NOARGS
#ifndef RR_BASE_EXT
int rr_test_outbase( void ) {
	char 
//			drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME],
	ext[_MAX_EXT] /*, *outfn = NULL, outfn2[_MAX_PATH] */;
	if ( global_opts.out_base ) {
		_splitpath( global_opts.out_base, NULL, NULL, NULL, ext );
		if ( ext[ 0 ] != '\0' ) {
			rr_printmsg( VERBOSE_QUIET,
				"mode2cdmaker basename %s: extension must be empty",
				global_opts.out_base );
			return ( FATAL_ERROR );
		}
//			outfn = outfn2;
//			_makepath( outfn, drive, dir, fname, RR_M2CDM_EXT );
	}
	return ( NO_ERROR );
}
#endif /* RR_BASE_EXT */
int rr_ferror( FILE *file, char *fname, char *comment ) {
	if ( file != NULL ) {
		int rc = ferror( file );
		if ( rc ) {
			rr_printmsg( VERBOSE_QUIET, "%s %s: write failed: %d - %s\n",
				comment, fname, rc, strerror( rc ) );
			return ( HARD_ERROR );
		}
	}
	return ( NO_ERROR );
}

#ifdef RR_CHECK_ISO1
int is_legal( const char *name ) {
	const unsigned char *word = (const unsigned char *) name;
	while ( *word != '\0' ) {
		if ( !( ( *word == ( unsigned char ) '_' ) || 
				( ( *word >= (unsigned char) '0' ) && ( *word <= (unsigned char) '9' ) ) ||
				( ( *word >= (unsigned char) 'A' ) && ( *word <= (unsigned char) 'Z' ) ) ||
				( ( *word >= (unsigned char) 'a' ) && ( *word <= (unsigned char) 'z' ) ) ) )
			return (0);
		word++;
	}
	return (1);
}
int rr_check_iso1( char *fullname ) {
	char dir[_MAX_DIR], name[_MAX_FNAME], ext[_MAX_EXT];
	char *ext2 = ext; int jx, namelen;
	_splitpath( fullname, NULL, dir, name, ext );
	namelen = strlen( name );
	if ( *ext2 == '.' )
		ext2++;
	if ( ( namelen == 0 ) || ( namelen > 8 ) || ( strlen( ext2 ) > 3 ) ||
		!is_legal( name ) || !is_legal( ext2 ) )
		return (0);	// is not complaint to iso level 1
	jx = strlen( dir );
//#error "циклит при jx == 1"
	while ( ( jx > 0 ) && ( dir[ jx - 1 ] == '\\' ) ) {
		dir[ --jx ] = '\0';
	}
	if ( jx < 1 )
		return (1);
	return ( rr_check_iso1( dir ) );
}
#ifdef RR_DEC_PATH
	#ifdef RR_IGNORE_DIR
char *rr_combo_name( char *outname, const char *base, const char *name ) {
	char t_name[ _MAX_FNAME ], t_ext[ _MAX_EXT ];
	_splitpath( name, NULL, NULL, t_name, t_ext );
	return ( rr_fullname(outname, base, t_name, t_ext, "target file", 0 ) );
}
	#endif /* RR_IGNORE_DIR */
#endif /* RR_DEC_PATH */

#endif /* RR_CHECK_ISO1 */
#endif /* RR_NOARGS */

#endif /* RR_TEST_ONLY */

// для отладки:
#ifdef MALLOC_DEBUG

#undef malloc
#undef free
#define PAGESIZE 4096

#ifdef __WIN32__ /*__WIN32__*/
#include <windows.h>
#endif /*__WIN32__*/

typedef struct {
	void *real_addr;
	size_t real_size;
	void *guard1, *guard2;
} guard_t;

void *rr_guard_malloc( size_t size ) {
//	guard_t guard, *pguard;
	void *real_addr, *returned_addr;
	size_t returned_size, real_size;

	real_size = ( size + 3 * PAGESIZE - 1 ) & ~( PAGESIZE - 1 );
//	returned_size = ( size + PAGESIZE - 1 ) & ~( PAGESIZE - 1 );
	real_addr = VirtualAlloc( NULL, real_size, MEM_RESERVE, PAGE_NOACCESS );
	if ( real_addr == NULL )
		return ( NULL );
	returned_addr = (void *) ( ( (unsigned long) real_addr ) + PAGESIZE );
	if ( VirtualAlloc( returned_addr, real_size - 2 * PAGESIZE, MEM_COMMIT,
			PAGE_READWRITE ) == NULL ) {
		VirtualFree( real_addr, 0, MEM_RELEASE );
		return ( NULL );
	}
	return ( returned_addr );
}
void rr_guard_free( void *addr ) {
	VirtualFree( (void *) ( ( (unsigned long) addr ) + PAGESIZE ), 0, MEM_RELEASE );
}

#endif /* MALLOC_DEBUG */

#ifndef RR_NOARGS
size_t rr_timeline( time_t delta, char *buf ) {
	size_t len;
#ifdef TIMELINE_LONG
	time_t hrs = delta / 3600, mins = ( delta / 60 ) % 60;
	char *min_fmt = "%d min ";

	if ( hrs ) {
		len = sprintf( buf, "%d hr ", (int) hrs ); buf += len; min_fmt = "%02d min ";
	}
	len = sprintf( buf, min_fmt, (int) mins ); buf += len;
	len = sprintf( buf, "%02d", (int) ( delta % 60 ) ); buf += len;
#else /* TIMELINE_LONG */
	len = sprintf( buf, "%d:%02d:%02d", (int) ( delta / 3600 ),
			(int) ( ( delta / 60 ) % 60 ), (int) ( delta % 60 ) );
	buf += len;
#endif /* TIMELINE_LONG */
	return ( len );
}

size_t rr_approx_time( char *buf, int readiness, time_t elapsed,
char *prefix ) {
	size_t len;
	time_t total, remains;
	char elap_buf[ 0x40 ], rema_buf[ 0x40 ];
	if ( readiness == 0 )
		return 0;

	if ( prefix == NULL )
		prefix = "";
	total = ( elapsed * FULL_READINESS ) / readiness;
	remains = total - elapsed;
	rr_timeline( elapsed, elap_buf );
	len = sprintf( buf, "%selapsed %s", prefix, elap_buf );
	if ( ( elapsed > 3 ) && ( readiness < FULL_READINESS ) ) {
		rr_timeline( remains, rema_buf );
		len += sprintf( buf + len, ", remains %s", rema_buf );
	}
	return ( len );
}
void rr_printstatus( int readiness, time_t start_time, int errors,
#ifdef RR_SHOW_LOST
	int lost,
#endif /* RR_SHOW_LOST */
char *suffix ) {
//	int readiness = rr_readiness( rr );
	time_t cur_time = time( NULL ) - start_time;
	char 
	timebuf[ 0x100 ] = "",
	errbuf[ 0x40 ], *errptr = errbuf,
#ifdef RR_SHOW_LOST
	lostbuf[ 0x40 ] = "",
#endif /* RR_SHOW_LOST */
	readybuf[ 0x10 ], *readyptr = readybuf;
#ifdef RR_BARE_OUTPUT
	#ifndef RR_TEST_ONLY
	if ( global_opts.bare_status ) {
		printf( "status %d %d %d %d %d\n",
			(int) start_time, (int) cur_time, readiness, errors, lost );
		return;
	}
	#endif /* RR_TEST_ONLY */
#endif /* RR_BARE_OUTPUT */
#ifdef RR_OUTPUT_REDIR
	if ( global_opts.out_redirected ) {
		suffix = "\n";
	}
#endif /* RR_OUTPUT_REDIR */
	if ( errors > 0 ) {
		sprintf( errbuf, ", %d error(s)", errors );
	} else if ( errors == 0 ) {
		errptr = ", no errors";
	} else {	// errors < 0 
		errptr = "";
	}
#ifdef RR_SHOW_LOST
	if ( lost > 0 )
		sprintf( lostbuf, ", %d lost sector(s)", lost );
#endif /* RR_SHOW_LOST */
	if ( readiness < FULL_READINESS ) {
		sprintf( readybuf, "%3d.%01d%%", readiness / 10, readiness % 10 );
	} else {
		readyptr = "100.0%";
	}

//	if ( cur_time > 3 ) {
//				sprintf( timebuf, ", %d secs remains", 
//					( ( FULL_READINESS - readiness )* cur_time ) / readiness );
	rr_approx_time( timebuf, readiness, cur_time, ", " );
//	}
	rr_printmsg( VERBOSE_QUIET, "%s%s" /*" completed"*/ "%s"
#ifdef RR_SHOW_LOST
		"%s"
#endif /* RR_SHOW_LOST */
		"%s%s",
#ifdef RR_OUTPUT_REDIR
		global_opts.out_redirected ? "" :
#endif /* RR_OUTPUT_REDIR */
		LONGBLANKLINE, readyptr, errptr, 
#ifdef RR_SHOW_LOST
		lostbuf,
#endif /* RR_SHOW_LOST */

		timebuf, suffix );
#ifdef RR_OUTPUT_REDIR
	if ( global_opts.out_redirected )
		fflush( stdout );
#endif /* RR_OUTPUT_REDIR */
//	printf( LONGBLANKLINE "%2d.%01d%% completed, %d errors%s %s", 
//			readiness / 10, readiness % 10, errors, timebuf, suffix );
}
int rr_printmsg( verbose_t verbose, char *fmt, ... ) {
	int clear = verbose & ~0x0FFF;
	int rc;
	va_list args; va_start( args, fmt );
#ifdef RR_BARE_OUTPUT
	#ifndef RR_TEST_ONLY
	if ( global_opts.bare_status ) {	// не выводить ничего?
		return (0);
	}
	#endif /* RR_TEST_ONLY */
#endif /* RR_BARE_OUTPUT */
	verbose &= 0x0FFF;
#ifndef RR_TEST_ONLY
	if ( verbose > global_opts.verbose_level )
		return ( 0 );
#endif /* RR_TEST_ONLY */
	if ( clear )
		printf( LONGBLANKLINE );
	rc = vprintf( fmt, args );
	fflush( stdout );
	return ( rc );
}
int rr_make_seclen( int type ) {
	switch ( type ) {
		default: {
				rr_printmsg( VERBOSE_QUIET, "Fatal error: illegal sector size!\n" );
				return ( FATAL_ERROR );
			}
		case 0: {
				return ( XCD_DATA_LEN );
			}
		case 1: {
				return ( ISO_LEN );
			}
	}
}
#ifdef RR_NEW_ENC
#if 0 /* и на фига я это впихнул ? */
int rr_readchain( rrchain_t *rrchain, char *buf,
int bufblks, int max_dose, rrtable_t *rr, int *error_count ) {
	int jx, jy = 0, total_blks = 0, buf_errs;
#ifdef RR_SHOW_LOST
	int total_lost = 0;
#endif /* RR_SHOW_LOST */
	time_t pass_time = time( NULL );
	if ( error_count ) *error_count = 0;
	rr_seek( rrchain, 0 );
	for ( total_blks = 0;; ) {
#ifdef RR_SHOW_LOST
		int readlost, buf_lost = 0;
#endif /* RR_SHOW_LOST */
		int readerrs;
		buf_errs = 0;
		for ( jx = 0; jx < bufblks * RR_STREAMNUM; jx += jy ) {
			int dose = bufblks * RR_STREAMNUM - jx;
			readerrs = 0;
			jy = rr_read( rrchain, buf + ( jx * OGM_LEN ),
					( dose > max_dose ) ? max_dose : dose, &readerrs
#ifdef RR_SHOW_LOST
					, &readlost
#endif /* RR_SHOW_LOST */
				);
//			if ( error_count ) *error_count += readerrs;
			buf_errs += readerrs;
#ifdef RR_SHOW_LOST
			buf_lost += readlost;
#endif /* RR_SHOW_LOST */
			if ( jy == 0 )
				break;	// eof
			else if ( jy <= FATAL_ERROR )
				return ( jy );	// goto abort;
			else if ( jy < NO_ERROR )
#ifndef RR_FILL_BAD
				jy = 1; // error_count++;
#else /* RR_FILL_BAD */
			jy = 0;
#endif /* RR_FILL_BAD */
		}
		total_blks += jx;
		if ( error_count ) *error_count += buf_errs;
#ifdef RR_SHOW_LOST
		total_lost += buf_lost;
#endif /* RR_SHOW_LOST */
		if ( jy == 0 ) {	// EOF
			int tail = jx % RR_STREAMNUM, busy = jx - tail;
			if ( rr != NULL )
				rr_decode( rr, busy / RR_STREAMNUM, (rrdata_t *) buf );
			rr_printstatus( rr_readiness( rrchain ), pass_time,
				( error_count == NULL ) ? -1 : *error_count,
#ifdef RR_SHOW_LOST
				total_lost,
#endif /* RR_SHOW_LOST */
				"\n" );
			break;
		}
		if ( rr != NULL )
			rr_decode( rr, bufblks, (rrdata_t *) buf );
		rr_printstatus( rr_readiness( rrchain ), pass_time,
			( error_count == NULL ) ? -1 : *error_count, 
#ifdef RR_SHOW_LOST
			total_lost,
#endif /* RR_SHOW_LOST */
			( ( error_count != NULL ) && ( ( buf_errs
#ifdef RR_SHOW_LOST
						+ buf_lost
#endif /* RR_SHOW_LOST */
					) > 0 ) ) ? "\n" : "" );
	}
	return ( total_blks );
}
int rr_check_result( rrchain_t *rrchain, rrtable_t *rr, char *buf,
char **errlocs, rritem_t *decoy ) {
/* -------------------------------------------------- *
 * returns:                                           *
 * NO_ERROR    - check OK (if any), recover OK        *
 * SOFT_ERROR  - check failed, recover not performed  *
 * HARD_ERROR  - both check and recover not performed *
 * FATAL_ERROR - output files must be deleted         *
 * -------------------------------------------------- */
	int
	jx, jy, rc, errcount = rr_errcount( rrchain ),
	*errnums, total_blks = rr_blks( rrchain );
	if ( rr_used_rrcount( rrchain, 1 ) > global_opts.errcount ) {
//		rr_printmsg( VERBOSE_QUIET, "\n\nToo many errors\n" );
		return ( HARD_ERROR );
	}
//	errlocs = (char **) ( buf + buflen );
	errnums = rr_errlocs( rrchain );
	for ( jx = 0; jx < errcount; jx++ ) {
		errlocs[ jx ] = buf + ( jx * OGM_LEN );
	}
	memset( buf, 0, errcount * OGM_LEN );
//#error "Неинициализированный массив errlocs"
	if ( rr_restore( rr, total_blks / RR_STREAMNUM, 
			OGM_LEN, errcount, errnums, (rrdata_t **) errlocs ) == 0 ) {
//low_rrdata:
		return ( HARD_ERROR );
	}
	if ( /*global_opts.check*/ decoy != NULL ) {
		int check_len = rr_blklen( rrchain );
		for ( jy = errcount - RR_STREAMNUM; jy < errcount; jy++ ) {	// всегда в конце
			char *check_buf = errlocs[ jy ];
			for ( jx = 0; jx < check_len; jx++ ) if ( check_buf[ jx ] != 0 ) {
					rr_printmsg( VERBOSE_QUIET, "Extra check FAILED - source files are UNUSABLE!\n" );
					return ( SOFT_ERROR );
				}
		}
		rr_printmsg( VERBOSE_NORMAL, "Extra check passed OK.\n" );
		errcount -= RR_STREAMNUM;
	}
	if ( !( CHECK_ONLY || ( /*global_opts.check*/
			/*( decoy != NULL ) &&*/ ( errcount == 0 ) ) ) ) {	// только обманка
		rr_printmsg( VERBOSE_NORMAL, "Writing repaired sectors " );
		for ( jx = 0; jx < errcount; jx++ ) {
			rr_seek( rrchain, errnums[ jx ] );
			if ( ( rc = rr_write( rrchain, errlocs[ jx ], 1 ) ) < NO_ERROR )
				return ( ( rc > HARD_ERROR ) ? HARD_ERROR : rc );
			else if ( rc != 1 )
				return ( FATAL_ERROR );
		}
	}
	return ( NO_ERROR );
}
#endif /*0*/
#ifdef RR_LEGALZERO
int rr_atoi( const char *inbuf ) {
	size_t jx = 0;
	if ( *inbuf == '-' )
		jx++;
	for ( ; jx < strlen( inbuf ); jx++ ) {
		if ( !isdigit( inbuf[ jx ] ) ) {
			return ( -2 );
		}
	}
	return ( atoi( inbuf ) );
}
#endif /* RR_LEGALZERO */

#endif /* RR_NEW_ENC */
#endif /* RR_NOARGS */
