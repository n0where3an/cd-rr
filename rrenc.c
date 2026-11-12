/*
TODO:
	- ключик для вывода в .um2 команд в длинной форме - для читабельности
	- на ходу генерировать список с .md5-суммами файлов
*/
#include "config.h"
//#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rr.h"
#ifdef __WIN32__
	#include "getopt.h"
#else
	#include <getopt.h>
#endif /*__WIN32__*/
#include "rrmath.h"
#include "rrfile.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __WIN32__
#include <io.h>
#ifdef RR_OEM_API
#include <windows.h>
#endif /* RR_OEM_API */
#else
    #include <sys/io.h>
    #include <errno.h>
    #include <unistd.h>
    #include <ctype.h>
#endif /*__WIN32__*/

#define RR_SOFTNAME "rrenc"
//#define RR_SOFTDESC "RR-XCD encode / protect"
#define RR_SOFTDESC "cd-R eRRoR coRRectoR encode / protect"
#define RR_SOFTVER "1.30"
#include "copyrite.h"
/* Optons:
	/nologo /Zp16 /MT /W4 /GX /Ot /Og /Ob1
	/D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"
	/FAcs /Fa"rrenc___Win32_Release/" /FR"rrenc___Win32_Release/"
	/Fp"rrenc___Win32_Release/rrenc.pch" /Fo"rrenc___Win32_Release/"
	/Fd"rrenc___Win32_Release/"
	/YX /FD /O3 /G7 /Qsox /Qopt_report_levelmax /Qopt_report /Qvec_report2
	/Qsfalign16 /GA /GF /Gs /QxW /Qunroll4 /c
*/

//#define OGM_LEN global_opts.ogm_len
#define BLKNUM 1024
//#define BLKNUM 5000

// прочие define'ы
#ifndef RR_ALLOW_ABSPATH
	#define NEED_ABSBASE
#else /* RR_ALLOW_ABSPATH */
	#define NEED_ABSBASE \
		( global_opts.process_dir != DIR_EXPLICIT ),	/* НЕ распяливать base-каталоги до абсолютных, */ \
								/* только если указан "--process-dir" */
#endif /* RR_ALLOW_ABSPATH */

opts_t global_opts;
char vol_label[ 0x20 ] = "";
#ifdef RR_STORE_EDC
	LIST_HEAD( global_list );	//сразу проинициализируем
#endif /* RR_STORE_EDC */

const struct option long_opts[] = {
	{ "help",           0, NULL, 'h' },	// он и в Африке help
	{ "quiet",          0, NULL, 'q' },
	{ "verbose",        0, NULL, 'v' },
	{ "overwrite",      0, NULL, 'w' },
	{ "fake-output",    0, NULL, 'k' },
	{ "no-read-files",  0, NULL, 'n' },
	{ "input-list",     1, NULL, '@' },
	{ "output-base",    1, NULL, 'o' },
	{ "source-dir",     1, NULL, 's' },
#ifdef RR_ENC_TGTBASE
	{ "target-dir",     1, NULL, 't' },
#endif /* RR_ENC_TGTBASE */
	{ "plain-list",     1, NULL, 'p' },
//	{ "iso",           1, NULL, 'i' },
//	{ "xcd",           1, NULL, 'x' },
	{ "rrdata-header",  1, NULL, 'r' },
	{ "volume-label",   1, NULL, 'l' },
	{ "form2-ext",      1, NULL, 'e' },
	{ "strict-mode",    0, NULL, 'i' },
#ifdef RR_NO_INIT_READ
	{ "no-check-hdr",   0, NULL, 'c' },
#endif /* RR_NO_INIT_READ */
#ifdef RR_OEM_API
	{ "ansi-charset",   0, NULL, 'a' },
#endif /* RR_OEM_API */
	{ "multi-track",    0, NULL, 'u' },
#ifdef RR_KEEP_OLD_EXT
	{ "keep-old-ext",   0, NULL, 'x' },
#endif /* RR_KEEP_OLD_EXT */
#ifdef RR_IGNORE_DIR
	{ "process-dir",    0, NULL, 'd' },
#endif /* RR_IGNORE_DIR */
#ifdef RR_STORE_EDC
	{ "keep-digest",    0, NULL, 'g' },
#endif /* RR_STORE_EDC */
	{ NULL,             0, NULL,  0  }
};

char gacmdline[INBUFLEN + 1];
int LSetCommandLine(char *jpcmdline,int argc, char **argv) {
	int lx, ll, la, lz, lj;
	char *jptr, *jptrp, *jptra, *jptrz, jc,jp=0;
	char jquote;
	jpcmdline [0]=0;
	ll = 0;
	for (lx = 0; lx < argc; lx++) {
		la = strlen (argv[lx]);
		jquote = 0;
		jptrz = jpcmdline+ll;
		//ищем "' '
		//ищем 0x22;0x27;0x20;
		lz = la;
		jptra = argv[lx];
		jptrp = 0;
		do {
			//Begin:Экранируемые слешем и кавычками
			//нужно не только факт наличия но и минимальная позиция
			jc = 0x22;
			jptr = (char*) memchr (jptra, jc , lz);//'"'
			if (!jptr) {
				jp = 0x27;
				jptrp = (char*) memchr (jptra, jp , lz);//'''
			}
			if (jptrp && ((jptr && jptrp < jptr)|| !jptr)) {
				jptr = jptrp;
				jc = jp;
			}
			if (!jptr) {
				jp = 0x60;
				jptrp = (char*) memchr (jptra, jp , lz);//'`'
			}
			if (jptrp && ((jptr && jptrp < jptr)|| !jptr)) {
				jptr = jptrp;
				jc = jp;
			}
			//End:Экранируемые слешем и кавычками
			//Begin:Экранируемые кавычками
			//нужно только факт наличия
			if (!jptr) {
				jp = 0x20;
				jptr = (char*) memchr (jptra, jp , lz);//' '
			}
			if (!jptr) {
				jp = 0x24;
				jptr = (char*) memchr (jptra, jp , lz);//'$'
			}
			//End:Экранируемые кавычками
			if (jptr) {
				//чевой-то нашли
				if (jptra == argv[lx] && !jquote) { //первое вхождение, надо вставить начальную кавычку
					*(jptrz++) = 0x22;
					la++;
					jquote = 1;
				}
			}
			if (jptr && jc != 0x20 && jc != 0x24 ) {
				lj  = jptr - jptra;
				strncpy(jptrz,jptra,lj);
				*(jptrz +=lj) = 0x5c ; // '\'
				*(++jptrz) = jc ; // ", '
				++jptrz;
				jptra = jptr +1; //сдвигаемся за обрамляемый символ
				lz -= lj+1;
				la++;
			} else {
				//копируем конец строки
				strncpy(jptrz,jptra,lz);
				if (jquote) {
					//копируем заключающую кавычку
					jptr = NULL;
					*(jptrz+=lz) = 0x22;
					*(++jptrz) = 0x0;
					jquote = 0;
					la++;
				}
			}
		}
		while (jptr!=NULL);
//			strncpy(jcmdline+ll,argv[lx],la);
		ll+=la;///
		*(jpcmdline+ll) = 0x20;
		ll++;
		*(jpcmdline+ll) = 0;
	}
	return strlen (jpcmdline);
}

const char *progname;
void copyright() {
	static int already = 0;
	if ( already )
		return;
	already = 1;
	printf( RR_COPYRIGHT );
}
void usage( void ) {
//	char drive[_MAX_DRIVE];
//	char dir[_MAX_DIR];
	char fname[_MAX_FNAME], *retname;
//	char ext[_MAX_EXT];
#ifdef __WIN32__
	_splitpath( progname, NULL, NULL, fname, NULL );
	retname = fname;
#else // __WIN32__
	retname = basename( progname );
#endif // __WIN32__
	copyright();
	rr_printmsg( VERBOSE_NORMAL,
		"Usage:\n%s "
#ifndef RR_HELP_OPTS
		"<switches> [options] "
#else /* RR_HELP_OPTS */
		"-@<file> [ -hwtknvq -s<path> "
	#ifdef RR_ENC_TGTBASE
		"t<path> "
	#endif /* RR_ENC_TGTBASE */
		"-o<name> -p<file>\n"
		"\t-e<ext> -l<label> -r<file> ] "
#endif /* RR_HELP_OPTS */
#ifndef RR_ENC_TGTBASE
	#define SRCTGTBASE	"source"
	#define SRCTGTABBR "-s"
#else /* RR_ENC_TGTBASE */
	#define SRCTGTBASE	"target"
	#define SRCTGTABBR "-t"
#endif /* RR_ENC_TGTBASE */


		"<RR-size> <RR-list> [RR-datafile]"
		"\n"
		"\n  RR-size      - recovery data size (in sectors) (must be > 0)"
		"\n  RR-list      - output list of protected files (in RRDEC format, see"
		"\n                 \"RRDEC -h\"; path is relative to " SRCTGTBASE "-dir"
#ifdef RR_ALLOW_ABSPATH
		" or may be"
		"\n                 an ABSOLUTE path if \"-d\" is specified and "SRCTGTBASE "-dir"
		"\n                 is empty"
#endif /* RR_ALLOW_ABSPATH */
		")"
		"\n  RR-datafile  - output recovery data filename"
#ifdef RR_ALLOW_ABSPATH
		"; path as above"
#endif /* RR_ALLOW_ABSPATH */
		"\n                 (default: none and implies \"-k\")"

		"\n\nSWITCHES (required):"
		"\n"
		"\n  -@, --input-list=<file>     - list of files to process (see format below)"
		"\n                                (NOT relative to source-dir)"

		"\nOPTIONS:"
		"\n"
		"\n  -h, --help                  - this help text"
		"\n  -s, --source-dir=<path>     - directory which contains source files"
		"\n                                (default: current dir)"
//			"\n                                (usually on CD-ROM)"
		"\n                                (ATTENTION: All file paths except ones that"
#ifndef RR_ALLOW_ABSPATH
		"\n                                specified explicitly about are relative"
		"\n                                to source-dir)"
#else /* RR_ALLOW_ABSPATH */
		"\n                                specified explicitly about are RELATIVE"
		"\n                                to source-dir in case when source-dir is"
		"\n                                empty and/or \"-d\" is NOT specified;"
		"\n                                otherwise, these paths can be absolute"
		"\n                                as well)"
#endif /* RR_ALLOW_ABSPATH */
#ifdef RR_ENC_TGTBASE
		"\n  -t, --target-dir=<path>     - directory for RR-list and RR-data files"
		"\n                                (NOT relative to source-dir; default: equal"
		"\n                                to source-dir)"
#endif /* RR_ENC_TGTBASE */
		"\n  -o, --output-base=<name>    - output base name for m2cdm files"
		"\n                                (NOT relative to source-dir, without"
		"\n                                extension, default: none)"
		"\n  -p, --plain-list=<file>     - generate list of all files in plain format"
		"\n                                (similar to \"dir /s /b /a-d\", good for"
		"\n                                making batch (.bat) files) (default: none;"
		"\n                                NOT relative to source-dir)"
		"\n  -e, --form2-ext=<ext>       - form2 file extension (as for m2cdm)"
		"\n                                (default \"" RR_DEFAULT_XCD_EXT "\")"
		"\n  -l, --volume-label=<label>  - volume label (for m2cdm's \"-v\")"
//			"\n                                (senseless without \"-o\" and RR-datafile)"
		"\n  -r, --rrdata-header=<file>  - binary header for RR-datafile"
		"\n                                (NOT relative to source-dir)"
		"\n  -w, --overwrite             - overwrite existing RR-datafile"
		"\n  -i, --strict-mode           - keep exact size/timestamp also for"
		"\n                                non-protected files (highly recommended"
		"\n                                for XCD)"
#ifdef RR_STORE_EDC
		"\n  -g, --keep-digest=<digest>  - keep simplified file digest into .rrt file"
		"\n                                (useful by rrdec for troubleshooting when"
		"\n                                redundancy check failed; see \"RRDEC -h\""
		"\n                                about \"-g\" switch in filelist format);"
		"\n                                currently, the only digest supported is"
		"\n                                EDC (\"-g edc\")"
#endif /* RR_STORE_EDC */
#ifdef RR_OEM_API
		"\n  -a, --ansi-charset          - assume ANSI charset for file names (for"
		"\n                                input-list and output RR-list contents;"
		"\n                                affects RUSSIAN letters in filenames;"
		"\n                                default: OEM/DOS charset)"
#endif /* RR_OEM_API */
#ifdef RR_KEEP_OLD_EXT
		"\n  -x, --keep-old-ext          - keep original file extension for XCD files"
		"\n                                (i.e. \"movie.avi\" becomes \"movie.avi." RR_DEFAULT_XCD_EXT "\";"
		"\n                                also breaks ISO Level 1 compatibility)"
#endif /* RR_KEEP_OLD_EXT */
		"\n  -u, --multi-track           - create multi-track XCD image"
		"\n                                (default: single-track)"
#ifdef RR_NO_INIT_READ
		"\n  -c, --no-check-hdr          - do NOT check whether file(s) is actually"
		"\n                                of mode2-form2 (may be useful to speed up"
		"\n                                init phase when directory being processed"
		"\n                                contains a lot of files)"
#endif /* RR_NO_INIT_READ */
#ifdef RR_IGNORE_DIR
		"\n  -d, --process-dir           - do NOT ignore \"-d\" directive from input"
	#ifndef RR_ALLOW_ABSPATH
		"\n                                list; otherwise, these directives will be"
		"\n                                ignored and paths from file description"
		"\n                                directives will be used"
	#else /* RR_ALLOW_ABSPATH */
		"\n                                list and also (if no \"-s\" specified)"
		"\n                                allow ABSOLUTE paths in file description"
		"\n                                directives; otherwise, \"-d\" directives"
		"\n                                will be ignored, and paths from file"
		"\n                                description directives will be used and"
		"\n                                interpreted as RELATIVE to source-dir"
	#endif /* RR_ALLOW_ABSPATH */
#endif /* RR_IGNORE_DIR */
		"\n  -k, --fake-output           - no calculations, just generate listfiles"
		"\n                                and fake (zero-filled) RR-datafile"
		"\n  -n, --no-read-files         - as \"-k\", but no read files performed"
		"\n  -v, --verbose               - tiresome mode (for debugging)"
		"\n  -q, --quiet                 - silent mode - only serious errors are"
		"\n                                displayed (for batch mode)"

		"\n\nINPUT LIST FORMAT: (NOTE: m2cdm's paramfile can be set with \"-o\")"
		"\n"
		"\n  # <any text>    - comment"
		"\n                  - empty line"
		"\n  -@              - place for RR-list name"
		"\n  -r              - place for RR-datafile name (must be located after"
		"\n                    last \"-i\" / \"-x\" line)"
		"\n  -l <label>      - volume label (for m2cdm's \"-v\"; have LESS priority"
		"\n                    than \"-l\" command-line option)"
		"\n  -c <parameter>  - extra m2cdm parameter (will be passed into"
		"\n                    paramfile AS IS)"
		"\n  -d <directory>"
#ifdef RR_IGNORE_DIR
		"  - is ignored when no \"-d\" command-line option is"
		""
		"\n                    specified; in this case, info about directories"
		"\n                    will be extracted from file paths"
#endif /* RR_IGNORE_DIR */
		"\n\nFile descriptions"
#ifdef RR_ALLOW_ABSPATH
		" (absolute paths is allowed ONLY if source-dir is"
		"\nempty and \"-d\" command-line option is specified)"
#endif /* RR_ALLOW_ABSPATH */
		" - corresponding"
		"\ndirectives for paramfile will be generated:\n"
//			"\n\nLines to be passed into m2cdm paramfile AS IS:"
		"\n  -f <ISO-file>   - form1 (ISO) file, no protection"
		"\n  -m <XCD-file>   - form2 (XCD) file, no protection"
		"\n  -i <ISO-file>   - form1 (ISO) file, will be RR-protected"
		"\n  -x <XCD-file>   - form2 (XCD) file, will be RR-protected"
		"\n" // "\nLines to be RR-processed and converted into m2cdm format:"
/*
			"\n  <process-type> <filename>"
			"\n\nProcess types:"
			"\n  -i      - form1 (ISO) file - will be converted into \"-f <filename>\""
			"\n  -x      - form2 (XCD) file - will be converted into \"-m <filename>\""
			"\n  -I, -X  - same as above but with alignment (optional)"
*/
#ifdef RR_ERRORLEVEL
		"\n\nERRORLEVEL CODES (for use in batch files):"
		"\n"
		"\n  %d  - success; all data is protected, everything is OK"
//			"\n  %d  - job is done well but there are warnings about possibly absent"
//			"\n       files or other troubles; please check RR-list generated"
//			"\n  %d  - redundancy check failed - improper RR-data; recover is impossible"
		"\n  %d  - fatal error is occurred (most probably there was out-of-space"
		"\n       on target drive, or out-of-memory) or error in parameters;"
		"\n       please check output, parameters list and filenames"
//			"\n  %d  - success"
#endif /* RR_ERRORLEVEL */
			"\n"
			"\nSee also:"
			"\nhttp://forum.doom9.org/showthread.php?t=93048 (english)"
			"\nhttp://forum.sharereactor.ru/showthread.php?t=59777 (russian)"
			"\n",
		retname
#ifdef RR_ERRORLEVEL
		, RR_RC_OK, /*RR_RC_WARNINGS, RR_RC_UNUSABLE,*/ RR_RC_FATAL /*,
			RR_RC_ILLEGAL_PARAM*/
#endif /* RR_ERRORLEVEL */
	);
}
int rr_savelist( rrchain_t *rrchain, FILE *outfile,
char *name, char *buf1, char *buf2, char **newdir ) {
//#error "Расширить на случай пустого имени файла"
//#error "Расширить на случай RRLIST_ITSELF"
	int size, time, blklen,
	flags = rr_lastfile_flags( rrchain );
	char *dir1 = buf1, *dir2 = buf2, lenmode;
	int dirlen, len; // fulllen, rc;
#ifdef RR_IGNORE_DIR
	char newname[_MAX_PATH];
#endif /* RR_IGNORE_DIR */

	if ( flags < 0 )
		return ( flags );
	if ( newdir != NULL )
		*newdir = NULL;

	blklen = rr_lastfile_blklen( rrchain );
#ifdef RR_FLIST_ITSELF
	if ( flags & RRLIST_ITSELF ) {
		lenmode = '@';
	} else
#endif /* RR_FLIST_ITSELF */
		lenmode = (char) ( ( flags & NO_RRPROCESS )
				? ( ( blklen < XCD_DATA_LEN ) ? 'f' : 'm' )
				: ( ( blklen == ISO_LEN ) ? 'i' : 'x' ) );
	if ( name == NULL )
		name = "";
	if ( name[ 0 ] == '\0' )
		goto noname;
#ifdef RR_IGNORE_DIR
	if ( global_opts.process_dir == DIR_EXPLICIT ) {
		name = rr_combo_name( newname, dir1, name );
		goto noname;	// need to ignore change basedir of file
	}
#endif /* RR_IGNORE_DIR */
	_splitpath( name, NULL, dir1, NULL, NULL );
	if ( ( dirlen = strlen( dir1 ) ) > 1 ) {
		char *dirend = dir1 + dirlen - 1;
		if ( *dirend == '\\' )
			*dirend = '\0';
	}
	if ( strcmp( dir1, dir2 ) ) {	// ё-пс! смена каталога
		char *tdir = dir1;
#ifndef RR_STORE_EDC
		fprintf( outfile, "-d %s\n", dir1 );
#endif /* RR_STORE_EDC */
		strcpy( dir2, dir1 );
		dir1 = dir2; dir2 = tdir;
		if ( newdir != NULL )
			*newdir = dir1;
	}
noname:
#ifndef RR_STORE_EDC
	size = rr_lastfile_size( rrchain );
	time = rr_lastfile_time( rrchain );
	if ( flags & NEED_ALIGN )
		lenmode = (char) toupper( lenmode );
	if ( ( flags & ( NO_STRICT | NO_RRPROCESS ) ) ==
		( NO_STRICT | NO_RRPROCESS ) ) {
		len = fprintf( outfile, "-%c %s\n", lenmode, name );
	} else {
		len = fprintf( outfile, "-%c %9d %10d %s\n", lenmode,
				( flags & NO_FILELEN ) ? -1 : (int) size, time, name );
	}
#else /* RR_STORE_EDC */
	#error "Придётся считать всё это в самом конце..."
#endif /* RR_STORE_EDC */
/*	if ( ferror( outfile ) ) {
err:
		rc = -errno; errno = 0;
		return ( rc );
	}
 */
	return ( 0 );
	/*
	fulllen = fprintf( outfile, "%d\n", rrchain->rrcount );
	if ( ferror( outfile ) ) {
err:
		rc = errno;
		printf( "filelist: write failed: %d - %s\n",
				rc, strerror( rc ) );
		return ( -rc );
	}
 */
}

#define SET_FILE_TYPE( type ) \
	do { \
		last_type = (type); \
	} while (0)

#define CHK_FILE_TYPE( type ) \
	do { \
		if ( last_type != (type) ) { \
			last_type = (type); \
			sprintf( newtype, "%s\n", (type) ? M2CDM_CMD_FILE : M2CDM_CMD_MOVIE ); \
		} \
	} while (0)

#define MAKE_SECLEN( type, seclen ) \
	do { \
		if ( ( seclen = rr_make_seclen( type ) ) <= 0 ) \
			goto err01; \
	} while (0)

#ifndef RR_UPDATE_DIR
	#define GEN_SRC_NAME \
		do { \
			outbuf = rr_lastfile_full_in_name( strict );  \
		} while (0)
	#define GEN_TGT_NAME( append ) \
		do { \
			outbuf = append ? \
					rr_lastfile_full_in_name( in ) : \
					rr_lastfile_full_out_name( in ); \
		} while (0)
#else /* RR_UPDATE_DIR */
	#define GEN_SRC_NAME \
		do { \
			outbuf = rr_full_in_name( item ); \
		} while (0)
	#define GEN_TGT_NAME( append ) \
		do { \
			outbuf = append ? \
					rr_full_in_name( item ) : \
					rr_full_out_name( item ); \
		} while (0)
#endif /* RR_UPDATE_DIR */

#ifdef RR_FLIST_ITSELF
	#define PRINTF_ITSELF( nolenflag ) \
		( ( (nolenflag) | RRLIST_ITSELF ) ? '@' : 'f' )
#else /* RR_FLIST_ITSELF */
	#define PRINTF_ITSELF( nolenflag ) 'f'
#endif /* RR_FLIST_ITSELF */

#ifndef RR_NO_INIT_READ
	#define GLOBAL_ATTRIBS_NO_INIT_READ 0
int init_noread;
#else /* RR_NO_INIT_READ */
	#define GLOBAL_ATTRIBS_NO_INIT_READ \
		( global_opts.init_noread ? NO_INIT_READ : 0 )
#endif /* RR_NO_INIT_READ */

#define PUT_FILE_DIR \
	do { \
		if ( newdir != NULL ) { \
			fprintf( m2cdm_file, M2CDM_CMD_DIR "\n%s\n", newdir ); \
			SET_FILE_TYPE( -1 ); \
			CHK_FILE_TYPE( t_type ); \
		} \
	} while (0)

#ifndef RR_IGNORE_DIR
	#define PUT_DIR_IF_NEED PUT_FILE_DIR
#else /* RR_IGNORE_DIR */
	#define PUT_DIR_IF_NEED \
		if ( global_opts.process_dir != DIR_EXPLICIT ) PUT_FILE_DIR
#endif /* RR_IGNORE_DIR */
#ifndef RR_STORE_EDC
	#define ALL_FILES
#else /* RR_STORE_EDC */
	#define ALL_FILES global_list,
#endif /* RR_STORE_EDC */

#define PUT_NAME_INTO_LISTS \
	do { \
		if ( m2cdm_file != NULL ) { \
			CHECK_ISO1; \
			PUT_DIR_IF_NEED; \
			fprintf( m2cdm_file, "%s%s\n", newtype, outbuf ); \
		} \
		if ( plain_file != NULL ) \
			fprintf( plain_file, "%s\n", outbuf ); \
	} while (0)


#define PUT_SRC_FILE( type, nolenflag ) \
	do { \
		char *newdir = NULL, newtype[ 0x10 ] = "", t_type = (type); \
		CHK_FILE_TYPE( t_type ); \
		if ( rrlist_file != NULL ) { \
			/* if ( global_opts.strict_mode ) { */ \
				int seclen = 0; \
				MAKE_SECLEN( t_type, seclen ); \
				if ( ( ( item = rr_append( ALL_FILES strict, seclen, rest, NULL, \
						-1, -1, ( global_opts.strict_mode ? 0 : NO_STRICT ) | \
						GLOBAL_ATTRIBS_NO_INIT_READ | NO_RRPROCESS | (nolenflag) ) ) \
						== NULL ) || \
						( rr_savelist( strict, rrlist_file, rest, dirbuf1, dirbuf2, \
						&newdir ) < NO_ERROR ) ) \
					goto err01; /* \
			} else { \
				fprintf( rrlist_file, "-%c %s\n", \
					( t_type ? PRINTF_ITSELF( nolenflag ) : 'm' ), rest ); \
			} */ \
		} \
		if ( ( m2cdm_file != NULL ) || ( plain_file != NULL ) ) { \
			GEN_SRC_NAME; \
			PUT_NAME_INTO_LISTS; \
		} \
	} while (0)

#ifndef RR_NEW_ENC
	#define CHECK_RRDATA_POS \
		do { \
			if ( put_rrdata_flag ) { \
				rr_printmsg( VERBOSE_QUIET, \
						"%s %s, line %d: -r/-i/-x line after -r line\n", \
						CONCAT_NAME( src, desc ), \
						global_opts.filelist, line ); \
				goto err01; \
			} \
		} while (0)
	#define CHECK_RRDATA_QTY \
		do {} while (0)
	#define SET_START_BLOCK \
		do {} while (0)

#else /* RR_NEW_ENC */
	#define CHECK_RRDATA_POS \
		do {} while (0)
	#define CHECK_RRDATA_QTY \
		do { \
			if ( put_rrdata_flag ) { \
				rr_printmsg( VERBOSE_QUIET, \
						"%s %s, line %d: -r line is superfluous\n", \
						CONCAT_NAME( src, desc ), \
						global_opts.filelist, line ); \
				goto err01; \
			} \
		} while (0)
	#define SET_START_BLOCK \
		do { \
			if ( global_opts.no_readfiles && ( rrdata != NULL ) ) \
					/* будем читать только RR-header */ \
				*rrdata = item; \
		} while (0)
#endif /* RR_NEW_ENC */

#define PUT_TGT_FILE( type, align, append ) \
	do { \
		char /* *tlen, *tdate, */ *newdir = NULL, newtype[ 0x10 ] = ""; \
		int seclen = 0, t_type = (type); \
		CHECK_RRDATA_POS; \
		MAKE_SECLEN( t_type, seclen ); \
		CHK_FILE_TYPE( t_type ); \
		/* fprintf( rrdec_file, "%s %d %d %s\n", first, */ \
				/* len, date, rest ); */ \
		if ( (append) && \
				( ( item = rr_append( ALL_FILES in, seclen, rest, NULL, -1, -1, \
				(align) ) ) == NULL ) ) \
			goto err01; \
		if ( rr_savelist( in, rrlist_file, rest, dirbuf1, dirbuf2, \
				&newdir ) < NO_ERROR ) \
			goto err01; \
		if ( ( m2cdm_file != NULL ) || ( plain_file != NULL ) ) { \
			GEN_TGT_NAME( append ); \
		} \
		PUT_NAME_INTO_LISTS; \
	} while (0)

#ifndef RR_FLIST_ITSELF
	#define RRLIST_FLAGS 0
#else /* RR_FLIST_ITSELF */
	#define RRLIST_FLAGS RRLIST_ITSELF
#endif /* RR_FLIST_ITSELF */

#define PUT_RRLIST_FILE \
	do { \
		char *rest = rrlist_fname; \
		PUT_SRC_FILE( 1, NO_FILELEN | RRLIST_ITSELF ); \
	} while (0)

#define PUT_RRDATA_FILE \
	do { \
		int blklen, flags; \
		char *rest; \
		OGM_LEN = rr_blklen( in ); \
		CHECK_RRDATA_QTY; \
		if ( ( item = rr_setrrdata( in, global_opts.rr_header, rrdata_fname, \
				rrcount, ( global_opts.overwrite ? ALLOW_OVERWRITE : 0 ) ) ) == NULL ) \
			goto err01; \
		flags = rr_lastfile_flags( in ); \
		rest = rrdata_fname; \
		if ( flags < NO_ERROR ) \
			goto err01; \
		blklen = rr_lastfile_blklen( in ); \
		PUT_TGT_FILE( ( blklen < XCD_DATA_LEN ), flags & NEED_ALIGN, 0 ); \
		put_rrdata_flag = 1; \
		SET_START_BLOCK; \
	} while (0)

#define CHECK_FERRORS \
	do { \
		CHECK_SPECIAL_FILE( m2cdm ); \
		CHECK_SPECIAL_FILE( plain ); \
		CHECK_SPECIAL_FILE( rrlist ); \
		/* if ( rr_ferror( m2cdm_file, m2cdm_fname, \
				"m2cdm paramfile" ) < NO_ERROR ) \
			goto err01; \
		if ( rr_ferror( plain_file, m2cdm_fname, \
				"plain list file" ) < NO_ERROR ) \
			goto err01; \
		if ( rr_ferror( rrlist_file, rrlist_fname, \
				"output RR-list file" ) < NO_ERROR ) \
			goto err01; */ \
	} while (0)

//#define INBUFLEN ( _MAX_PATH + 64 )
#ifndef RR_ENC_TGTBASE
	#define ENC_TGTBASE global_opts.src_base
#else /* RR_ENC_TGTBASE */
	#define ENC_TGTBASE global_opts.tgt_base
#endif /* RR_ENC_TGTBASE */

rrchain_t * rrenc_prepare( char *rrdata_fname, int rrcount, char *rrlist_fname
#ifdef RR_NEW_ENC
	, rritem_t **rrdata
#endif /* RR_NEW_ENC */
) {
	FILE *src_file = NULL;
	int rc, line = 0; // , jx, jy;
	char inbuf[ INBUFLEN + 1 ];
	char *outbuf;
	char *m2cdm_fname = global_opts.out_base;
	char *plain_fname = global_opts.plain_listfile;
	char *src_fname = global_opts.filelist;
#ifdef RR_CHECK_ISO1
	int is_iso1 = 1;
#endif /* RR_CHECK_ISO1 */
#ifdef RR_KEEP_OLD_EXT
	int is_keepext = 0;
#endif /* RR_KEEP_OLD_EXT */

	FILE *rrlist_file = NULL, *m2cdm_file = NULL, *plain_file = NULL;
	rrchain_t *in = NULL, *strict = NULL;
	int last_type = -1 /*, max_seclen = 0 */;
	char dirbuf1[ _MAX_DIR ] = "", dirbuf2[ _MAX_DIR ] = "";
	int put_rrdata_flag = 0;
	rritem_t *item = NULL;
	M2CDM_FILE_BUFFERS;
	PLAIN_FILE_BUFFERS;
	RRLIST_FILE_BUFFERS( "output RR-list file" );

//	RRDATA_FILE_BUFFERS;
	SRC_FILE_BUFFERS( "source list file" );

//#error "Открывать файл ТОЛЬКО через rr_fullname от src_base"
//#error "Осталась фигня - RRDATA_FILE сделать через ту же макру"
//#error "Выделить общую часть из CREATE_SPECIAL_FILE и OPEN_SPECIAL_FILE"
//#error "потом как-то присобачить для генерации имён"
	char cmdline[ INBUFLEN - 4 ];
#ifdef __WIN32__
    #ifndef __WATCOMC__
	strncpy( cmdline, GetCommandLine(), INBUFLEN - 5 );
    #else /* __WATCOMC__ */
	strncpy( cmdline, getcmd(), INBUFLEN - 5 );
    #endif /* __WATCOMC__ */
#else
	strncpy( cmdline, gacmdline , INBUFLEN - 5 );
#endif
	cmdline[ INBUFLEN - 5 ] = '\0';
#if defined (RR_OEM_API) && (__WIN32__)
	if ( !global_opts.ansi_api ) {
		ANSI2OEM( cmdline );
	}
#endif /* RR_OEM_API */

	OPEN_SPECIAL_FILE( src, NULL, NULL );
// создаём выходной файл:
	CREATE_SPECIAL_FILE( rrlist, NULL, ENC_TGTBASE );
	if ( rrlist_file != NULL ) {
		fprintf( rrlist_file, "%d\n"
			"# This file is generated by " RR_SOFTNAME " " RR_SOFTVER "\n"
			"# Please DO NOT EDIT unless you know exactly what you want!\n\n"
			"# Command-line used:\n# %s\n\n"
#ifdef RR_OEM_API
			"%s"
#endif /* RR_OEM_API */
			"%s%s",
			rrcount, cmdline,
#ifdef RR_OEM_API
			global_opts.ansi_api ? "-a\n" : "",
#endif /* RR_OEM_API */
			global_opts.strict_mode ? "-t\n" : "",
			global_opts.multitrack ? "-u\n" : "" );
#ifdef RR_CUSTOM_XCD_EXT
		if ( global_opts.xcd_ext != RR_DEFAULT_XCD_EXT )
			fprintf( rrlist_file, "-e %s\n", global_opts.xcd_ext );
#endif /* RR_CUSTOM_XCD_EXT */
		if ( global_opts.volume )
			fprintf( rrlist_file, "-l %s\n", global_opts.volume );
		CHECK_SPECIAL_FILE( rrlist );
	}
// разбор файла:
	if ( ( in = rr_create( "RRENC", global_opts.xcd_ext, /* rrcount */ 0,
					global_opts.src_base,
			/* global_opts.tgt_base */ /*NULL*/ ENC_TGTBASE,
#ifdef RR_UPDATE_DIR
//#error "Кот не валялся"		// и не надо
					NULL,
#endif /* RR_UPDATE_DIR */
					NEED_ABSBASE
					global_opts.verbose_level ) )
		== NULL ) {
		goto err01;
	}
//	if ( global_opts.strict_mode ) {
	if ( ( strict = rr_create( "STRICT", global_opts.xcd_ext, 0,
					global_opts.src_base, /*NULL*/ global_opts.tgt_base,
#ifdef RR_UPDATE_DIR
					NULL,
#endif /* RR_UPDATE_DIR */
					NEED_ABSBASE
					global_opts.verbose_level ) ) == NULL )
		goto err01;

//	}
// создаём выходной файл:
	CREATE_M2CDM_FILE;
	CREATE_PLAIN_FILE;

	while ( ( rc = rrc_fgets( src_file, global_opts.filelist,
					&line, inbuf, INBUFLEN ) ) > 0 ) {
//		char *err_src, *err_file;
#ifdef __WIN32__
		char *first = strtok( inbuf, " \t\n" );
#else
		char *first = strtok( inbuf, " \t\r\n" );
#endif
//		int len, align;
		if ( first == NULL ) {
		no_token:
			if ( rrlist_file != NULL )
				fputs( "\n", rrlist_file );
			continue;
		}
		switch ( first[ 0 ] ) {
			case '#':	// комментарий - не трогаем
				if ( rrlist_file != NULL )
					fprintf( rrlist_file, "%s %s", first, strtok( NULL, "\n" ) );
//					fputs( inbuf, rrlist_file );
			case '\0':	// пустая строка - игнорируем
				goto no_token;
			default: {
					goto sw_err;
				} break;
			case '-': {
					char *rest; //, rrdec_key = 'x';
//				FILE *t_file = rrdec_file;
					if ( strlen( first ) != 2 ) {
					sw_err:
						rr_printmsg( VERBOSE_QUIET,
							"%s %s, line %d: invalid parameter(s)\n", CONCAT_NAME( src, desc ),
							global_opts.filelist, line );
						rc = -1;
						goto err01;
					}
#ifdef __WIN32__
					rest = strtok( NULL, "\t\n" );
#else
					rest = strtok( NULL, "\t\r\n" );
#endif
					if ( rest == NULL ) switch ( first[ 1 ] ) {	// ничего нет, кроме ключа
							default: {
									goto sw_err;
								} break;
							case '@': {
									PUT_RRLIST_FILE;
								} break;
							case 'r': {
									PUT_RRDATA_FILE;
								} break;
						} else switch ( first[ 1 ] ) {
							default: {
									goto sw_err;
								} break;
							case 'd': {	// каталог
#ifndef RR_IGNORE_DIR
									SET_FILE_TYPE( -1 );
									if ( m2cdm_file != NULL )
										fprintf( m2cdm_file, M2CDM_CMD_DIR "\n%s\n", rest );
									if ( rrlist_file != NULL )
										fprintf( rrlist_file, "-d %s\n", rest );
									strcpy( dirbuf1, rest ); strcpy( dirbuf2, rest );
#else /* RR_IGNORE_DIR */
									if ( global_opts.process_dir == DIR_EXPLICIT ) {
//#error "Продолжить здесь"
										SET_FILE_TYPE( -1 );
										if ( m2cdm_file != NULL )
											fprintf( m2cdm_file, M2CDM_CMD_DIR "\n%s\n", rest );
										if ( rrlist_file != NULL )
											fprintf( rrlist_file, "-d %s\n", rest );
										strcpy( dirbuf1, rest ); strcpy( dirbuf2, rest );
									}
#endif /* RR_IGNORE_DIR */
								} break;
							case 'f':
							case 'm': {	// form-2 file
									PUT_SRC_FILE( first[ 1 ] == 'f', 0 );
								} break;
//					case 'X':		// RR-protected form-2 aligned
							case 'x':		// RR-protected form-2
#ifdef RR_KEEP_OLD_EXT
								is_keepext = global_opts.keep_ext;
#endif /* RR_KEEP_OLD_EXT */
//					case 'I':		// RR-protected form-1 aligned
							case 'i':	{	// RR-protected form-1
									PUT_TGT_FILE( ( first[ 1 ] == 'i' ) /*|| ( first[ 1 ] == 'I' )*/,
								/*isupper( first[ 1 ] ) ? NEED_ALIGN :*/ 0, PUT_NEED_APPEND );
								} break;
							case 'l': {	// volume label
									if ( global_opts.volume == NULL ) {
										strncpy( vol_label, rest, sizeof( vol_label ) - 1 );
										vol_label[ sizeof( vol_label ) - 1 ] = 0;
										global_opts.volume = vol_label;
										SET_FILE_TYPE( -1 );
										if ( rrlist_file != NULL )
											fprintf( rrlist_file, "-l %s\n", rest );
										if ( m2cdm_file != NULL )
											fprintf( m2cdm_file, "-volume\n%s\n", rest );
									}
								} break;
							case 'c': {	// явный параметр для m2cdm
									SET_FILE_TYPE( -1 );
									if ( rrlist_file != NULL )
										fprintf( rrlist_file, "-c %s\n", rest );
									if ( m2cdm_file != NULL )
										fprintf( m2cdm_file, "%s\n", rest );
								} break;
						}
//				if ( rr_append( in, len, rest, NULL, -1, -1, align ) == NULL )
//					goto err01;
				} break;
		}
		CHECK_FERRORS;
	}
//eof:
	if ( !put_rrdata_flag ) {
		PUT_RRDATA_FILE;
//		CHECK_FERRORS;
	}
#ifdef RR_CHECK_ISO1
	if ( m2cdm_file != NULL ) {
		if ( is_iso1
#ifdef RR_KEEP_OLD_EXT
			&& !is_keepext
#endif /* RR_KEEP_OLD_EXT */
			)
			fprintf( m2cdm_file, M2CDM_CMD_ISO1 "\n" );
		else
			rr_printmsg( VERBOSE_NORMAL,
				"Warning: Some files are NOT compatible with ISO Level 1\n" );
	}
#endif /* RR_CHECK_ISO1 */
	fprintf( rrlist_file, "--\n" );	// конец списка
	rr_print( in );
#if ( defined( RR_OUTPUT_REDIR ) && !defined( NDEBUG ) )
#ifndef NDEBUG
	if ( global_opts.out_redirected )
#endif /* NDEBUG */
		fflush( stdout );
#endif /* RR_OUTPUT_REDIR, !NDEBUG */

	CHECK_FERRORS;
	rc = 0; goto exit;
err01:
	rc = 1;
	if ( in ) {
		rr_discard( in );
		in = NULL;
	}
exit:
	if ( strict ) {
		rr_discard( strict );
		strict = NULL;
	}
	if ( src_file != NULL )
		fclose( src_file );
	if ( rrlist_file != NULL )
		fclose( rrlist_file );
	if ( m2cdm_file != NULL )
		fclose( m2cdm_file );
	if ( plain_file != NULL )
		fclose( plain_file );
	return ( in );
}

//int time_flag = 0;
int main( int argc, char **argv ) {
//	time_t time1, time2;
	time_t start_time;
	rrchain_t *in = NULL; //, *strict = NULL;
	int out = -1;
	int rc = 1;
	int rrcount, /*rrlen, baselen,*/  bufblks, buflen;
	char *buf = NULL;
	int jx, jy, busy;
	rrtable_t *rr = NULL;
//	char parmkey;
	char *rrdata_fname = NULL;
	int start_block = 0;
#ifdef RR_NEW_ENC
	int read_len = -1;
	char **errlocs;
	rrdata_t *math_buf = NULL;
	rritem_t *rrdata;
#endif /* RR_NEW_ENC */
//	char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME],
//		ext[_MAX_EXT], *outfn = NULL, outfn2[_MAX_PATH], *m2cdm_fname = NULL;

//char drive[_MAX_DRIVE], dir[_MAX_DIR], name[_MAX_FNAME], ext[_MAX_EXT];
//_splitpath( "D:\\path1\\path2\\" /*".name.ext1"*/ ".ext2", drive, dir, name, ext );

//	rr_check_iso1( "\\\\server\\path1\\path2\\name.ext\\ff.nm" );

	progname = argv[ 0 ];
//	DebugBreak();
#ifdef __WIN32__
	SetConsoleTitle( RR_SOFTNAME ":" RR_SOFTDESC RR_SOFTVER );
#else
	LSetCommandLine(gacmdline,argc,argv);
#endif /* __WIN32__ */

//	printf( RR_COPYRIGHT );

	memset( &global_opts, 0, sizeof( global_opts ) );
	global_opts.program = PROGRAM_RRENC;
//	global_opts.ogm_len = XCD_DATA_LEN;
	global_opts.verbose_level = VERBOSE_NORMAL;
	if ( ( jx = rr_parse_args( argc, argv, long_opts ) ) < 0 ) {	// кривые аргументы
	wrong_cmdline:
		copyright();
		printf( "Illegal command line" );
	wrong_param:
		printf( " -- use -h for help.\n\n" );
//		usage();
		return ( 1 );
	}

	jx--;
	argv += jx; argc -= jx;

	if ( ( argc < 3 ) || ( argc > 4 ) )
		goto wrong_cmdline;
	if ( global_opts.verbose_level > VERBOSE_NORMAL )
		copyright();

	if ( argc > 3 )
		rrdata_fname = argv[ 3 ];
	else
		global_opts.fake_output = 1;

	if ( global_opts.filelist == NULL ) {
		copyright();
		printf( "Required option is not set" );
		goto wrong_param;
	}
/*
	if ( global_opts.plain_listfile != NULL ) {
		printf( "Sorry, \"--plain-list\" is not applied yet" );
					goto wrong_param;
				}
				*/
#if defined ( RR_OEM_API ) && defined( __WIN32__)
	if ( !global_opts.ansi_api ) {
		ANSI2OEM( global_opts.src_base );
		ANSI2OEM( global_opts.tgt_base );
		ANSI2OEM( global_opts.filelist );
		ANSI2OEM( global_opts.xcd_ext );
		ANSI2OEM( global_opts.volume );
		ANSI2OEM( global_opts.out_base );
		ANSI2OEM( global_opts.rr_header );
		ANSI2OEM( global_opts.plain_listfile );
		ANSI2OEM( rrdata_fname );
		ANSI2OEM( argv[ 2 ] );
		SetFileApisToOEM();
	}
#endif /* RR_OEM_API __WIN32__ */

	if ( global_opts.xcd_ext == NULL )
		global_opts.xcd_ext = RR_DEFAULT_XCD_EXT;

	rrcount = rr_atoi( argv[ 1 ] );	// размер RR-файла
	if ( ( rrcount <= 0 ) || ( rrcount > MAX_REDUNDANCY ) ) {
		copyright();
		printf( "Incorrect size of RR-data" );
		goto wrong_param;
	}
#ifndef RR_BASE_EXT
	if ( rr_test_outbase() )
		goto wrong_param;
#endif /* RR_BASE_EXT */

#ifdef RR_ENC_TGTBASE
#ifndef RR_ALLOW_ABSPATH
	if ( global_opts.src_base == NULL )
		global_opts.src_base = ".";
#else /* RR_ALLOW_ABSPATH */
	if ( global_opts.src_base == NULL ) {
	src_empty:
		global_opts.src_base = "";	// explicitly empty
//#error "Нельзя оставлять NULL - rr_create() / rr_fullname() съезжают с фазы"
		if ( global_opts.process_dir == DIR_EXPLICIT )
			rr_printmsg( VERBOSE_INFO, "Absolute paths in source list is now allowed.\n" );
//	} else if ( global_opts.src_base[ 0 ] == '\0' ) {
//		global_opts.src_base = NULL;
//		goto src_empty;
	}
#endif /* RR_ALLOW_ABSPATH */
	if ( global_opts.tgt_base == NULL )
		global_opts.tgt_base = global_opts.src_base;
#endif /* RR_ENC_TGTBASE */

	rrcount /= RR_STREAMNUM;

	if ( ( in = rrenc_prepare( rrdata_fname, rrcount * RR_STREAMNUM,
					argv[ 2 ]
#ifdef RR_NEW_ENC
					, &rrdata
//#error "Продолжим отсюда"
#endif /* RR_NEW_ENC */
				) ) == NULL )
		goto abort;
/*
	if ( ( in = rr_open( argv + 3, argc - 3, 0, NULL ) ) == NULL ) {
		usage( progname );
		goto err;
	}
	*/
#ifndef RR_NEW_ENC
	if ( global_opts.no_readfiles )	// будем читать только RR-header
		start_block = rr_lastfile_tell( in );
#else /* RR_NEW_ENC */
	if ( global_opts.no_readfiles ) {
		start_block = rr_get1st_blk( rrdata );
		read_len = rr_getblks( rrdata );
#error "Этим скопируем только header, потом надо будет rr_write'ить нули"
	}
#endif /* RR_NEW_ENC */
	bufblks = BLKNUM;
	if ( rrcount > bufblks )
		bufblks = rrcount;
	buflen = bufblks * ( OGM_LEN * RR_STREAMNUM );

	if ( ( buf = malloc( buflen
#ifdef RR_NEW_ENC
					+ rrcount * RR_STREAMNUM * sizeof( void* )
#endif /* RR_NEW_ENC */
				) ) == NULL ) {
		rr_printmsg( VERBOSE_QUIET, "Insufficient memory available\n" );
		goto abort;
	}
#ifdef RR_NEW_ENC
	errlocs = (char **) ( buf + buflen );
	math_buf = (rrdata_t *) buf;
#endif /* RR_NEW_ENC */
	if ( !global_opts.fake_output &&
		( ( rr = rr_init( /* RR_BASELEN, 45, */ rrcount, OGM_LEN * RR_STREAMNUM ) )
			== NULL ) )
		goto abort;
	printf( "\n" );
	rr_seek( in, start_block );	// чтобы читать только RR-header при --no-read-files

	start_time = time( NULL );
	for (;;) {
//		int readiness;
		for ( jx = 0, jy = -1; jx < bufblks * RR_STREAMNUM; jx += jy ) {
			jy = rr_read( in, buf + ( jx * OGM_LEN ),
					bufblks * RR_STREAMNUM - jx, NULL
#ifdef RR_SHOW_LOST
					, NULL
#endif /* RR_SHOW_LOST */
				);
			if ( jy == 0 )
				break;
			if ( jy < NO_ERROR ) {
//				jy = 1;
			abort:
				printf( "\nExecution terminated due to fatal error.\n" );
				rc = RR_RC_FATAL;
				goto err;
			}
		}
		if ( jy == -1 )
			goto abort;
		if ( jy == 0 ) {
			int
			tail = jx % RR_STREAMNUM;
			busy = jx - tail;
			if ( tail > 0 ) {
				tail = RR_STREAMNUM - tail;
				memset( buf + jx * OGM_LEN, 0, OGM_LEN * tail );
				busy += RR_STREAMNUM;
			}
			if ( !global_opts.fake_output )
				rr_encode( rr, busy / RR_STREAMNUM, (rrdata_t *) buf );
			rr_printstatus( rr_readiness( in ), start_time, -1,
#ifdef RR_SHOW_LOST
				0,
#endif /* RR_SHOW_LOST */
				"\n" );
//			printf( LONGBLANKLINE "100%% completed.        \n" );
			break;
		}
		if ( !global_opts.fake_output )
			rr_encode( rr, bufblks, (rrdata_t *) buf );
//		readiness = rr_readiness( in );
		rr_printstatus( rr_readiness( in ), start_time, -1,
#ifdef RR_SHOW_LOST
			0,
#endif /* RR_SHOW_LOST */
			"" );
//			printf( LONGBLANKLINE "%2d.%01d%% completed... ",
//				readiness / 10, readiness % 10 );
	}
//	rr_close( in ); in = NULL;
	if ( !global_opts.fake_output )
		rr_dump( rr, rrcount, 0, (rrdata_t *) buf );
	else
		memset( buf, 0, rrcount * RR_STREAMNUM * OGM_LEN );
// вывод полученного:
	if ( rr_putrrdata( in, buf ) != ( rrcount * RR_STREAMNUM ) ) {
		rr_printmsg( VERBOSE_QUIET,
			"There was an error writing recovery data file\n" );
		goto abort;
	}
	if ( !global_opts.fake_output )
		rr_cleanup( rr );
	rr = NULL;
	rr_close( in ); in = NULL;
//	time( &time2 );
	rc = 0;
//	if ( time_flag )
//		printf( "Working time: %5d secs\n", ( int ) difftime( time2, time1 ) );

err:
	if ( rr != NULL )
		rr_cleanup( rr );
	if ( buf != NULL )
		free( buf );
	if ( in != NULL )
		rr_close( in );
	if ( out != -1 )
		_close( out );
	return ( rc );
//#error "Дорисовать вывод подсказки о кодах завершения"
}
