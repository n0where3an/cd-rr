/* SPDX-License-Identifier: AGPL-3.0-or-later */
/*
TODO:
	- подключать файл с контрольными суммами, проверять файл по ним
	  (или хотя бы предупреждать пользователя о непригодности файла)
	- если много сбоев - перечитывать потенциально-сбойные участки по одному сектору
	- при чтении короткого файла - сразу открывать ещё пачку, чтобы было быстрее
	- правильно обрабатывать --no-restore (не-а! - будет другой ключ)
	(уже) - вывод elapsed/estimated time
	(уже) - тестирование (корректность восстановления)
	- грамотную проверку взаимоисключающих ключей (придумать)
*/
#include "config.h"
//#ifdef __WIN32__
//	#include <tchar.h>
//#endif

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
	#include <ctype.h>
#endif /*__WIN32__*/
#include "rrmath.h"
#include "rrfile.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __WIN32__
#include <io.h>
#else
	#include <sys/io.h>
	#include <errno.h>
#endif
#include <assert.h>
#ifdef __WIN32__
#include <windows.h>
#endif /* __WIN32__ */

#define RR_SOFTNAME "rrdec"
#define RR_SOFTDESC "cd-R eRRoR coRRectoR decode / recover"
//#define RR_SOFTDESC "           RR-XCD decode / recover           "
#define RR_SOFTVER "1.30"
#include "copyrite.h"

/* Optons:
	/nologo /Zp16 /MD /W4 /GX /Ot /Og /Ob1
	/D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"
	/FAcs /Fa"rrdec___Win32_Release/" /FR"rrdec___Win32_Release/"
	/Fp"rrdec___Win32_Release/rrdec.pch" /Fo"rrdec___Win32_Release/"
	/Fd"rrdec___Win32_Release/"
	/YX /FD /O3 /G7 /Qsox /Qopt_report_levelmax /Qopt_report /Qvec_report2
	/Qsfalign16 /GA /GF /Gs /QxW /Qunroll4 /c
*/

//#define BLKNUM 100
#define BLKNUM 1024
//#define BLKNUM 4096

#define PROBE_LEN_MIN 2
#define PROBE_LEN_MAX 2048
#define PROBE_LEN_DEFAULT 64
//#define PROBE_RRCOUNT 10 /* от фонаря */
#define OUT_BLKLEN 24
#define ABS_MAX_FOR_1PASS 12 /* если сбоев больше ( MAX_FOR_1PASS * 6 ) - на первом проходе check не считать */
#ifndef RR_1PASS_COUNT
	#define MAX_FOR_1PASS \
		( ( global_opts.errcount < ABS_MAX_FOR_1PASS ) ? \
		global_opts.errcount : ABS_MAX_FOR_1PASS )
#else /* RR_1PASS_COUNT */
	#define MAX_FOR_1PASS global_opts.max_for_1pass
#endif /* RR_1PASS_COUNT */

// прочие define'ы
#ifndef RR_ALLOW_ABSPATH
	#define NEED_ABSBASE
#else /* RR_ALLOW_ABSPATH */
	#define NEED_ABSBASE	1,	/* всегда распяливать base-каталоги до абсолютных */
#endif /* RR_ALLOW_ABSPATH */

const struct option long_opts[] = {
	{ "help",          0, NULL, 'h' },	// он и в Африке help
	{ "quiet",         0, NULL, 'q' },
	{ "verbose",       0, NULL, 'v' },
	{ "overwrite",     0, NULL, 'w' },
	{ "all-files",     0, NULL, 'a' },
#ifdef RR_NO_RESTORE
	{ "no-restore",    0, NULL, 'n' },
#endif /* RR_NO_RESTORE */
	{ "xcd-output",    0, NULL, 'x' },
#ifndef RR_CHECK_ALWAYS
	{ "extra-check",   0, NULL, 'c' },
#endif /* RR_CHECK_ALWAYS */
	{ "input-list",    1, NULL, '@' },
	{ "output-base",   1, NULL, 'o' },
	{ "source-dir",    1, NULL, 's' },
	{ "target-dir",    1, NULL, 't' },
	{ "form2-ext",     1, NULL, 'e' },
	{ "volume-label",  1, NULL, 'l' },
//	{ "iso",           0, NULL, 'i' },
//	{ "m2cdmparm",     1, NULL, 'c' },
//	{ "redundancy",    2, NULL, 'r' },
//	{ "xcd",      1, NULL, 'x' },
	{ "iso-input",     0, NULL, 'i' },
	{ "zero-damaged",  0, NULL, 'z' },
#ifdef RR_1PASS_COUNT
	{ "on-the-fly",    1, NULL, 'f' },
#endif /* RR_1PASS_COUNT */
#ifdef RR_KEEP_BAD
	{ "keep-broken",   0, NULL, 'b' },
#endif /* RR_KEEP_BAD */
#ifdef RR_KEEP_EXISTING
	{ "keep-existing", 0, NULL, 'k' },
#endif /* RR_KEEP_EXISTING */
#ifdef RR_UPDATE_DIR
	{ "update-dir",    1, NULL, 'u' },
	{ "update-protected",
										 0, NULL, 'r' },
#endif /* RR_UPDATE_DIR */
#ifdef RR_COPY_ONLY
	{ "copy-only",     0, NULL, 'p' },
#endif /* RR_COPY_ONLY */
#ifdef RR_NO_INIT_READ
	{ "no-check-hdr",  0, NULL, 'c' },
#endif /* RR_NO_INIT_READ */
	{ NULL,            0, NULL,  0  }
};

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
	const char *retname;
#ifdef __WIN32__
	char fname[_MAX_FNAME];
#endif // __WIN32__
//	char ext[_MAX_EXT];
#ifdef __WIN32__
	_splitpath( progname, NULL, NULL, fname, NULL );
	retname = fname;
#else // __WIN32__
	retname = strrchr( progname, '/' );	// basename() may modify its argument
	retname = ( retname == NULL ) ? progname : retname + 1;
#endif // __WIN32__
//	SetConsoleOutputCP( 866 );
	copyright();
	rr_printmsg( VERBOSE_QUIET,
//			"Проверка русских букв\n\n"
			"Usage:\n%s "
#ifndef RR_HELP_OPTS
			"<switches> [options]\n"
#else /* RR_HELP_OPTS */
			"-@<file> [ -hw"
	#ifdef RR_KEEP_EXISTING
			"k"
	#endif /* RR_KEEP_EXISTING */
			"nczbvq "
	#ifdef RR_1PASS_COUNT
			"-f<sectors> "
	#endif /* RR_1PASS_COUNT */
	#ifdef RR_DMG_LIST
			"-d<file> "
	#endif /* RR_DMG_LIST */
			"-ax\n\t-s<path> -t<path> "
	#ifdef RR_UPDATE_DIR
			"-u<path> "
	#endif /* RR_UPDATE_DIR */
			"-o<name> -e<ext> -l<label> ]\n"
#endif /* RR_HELP_OPTS */
			"\n(or in old (version before 0.98 compatible) mode:"
			"\n%s [ "
#ifndef RR_HELP_OPTS
			"options "
#else /* RR_HELP_OPTS */
			"-hw"
	#ifdef RR_KEEP_EXISTING
			"k"
	#endif /* RR_KEEP_EXISTING */
			"nczbvq "
	#ifdef RR_1PASS_COUNT
			"-f<sectors> "
	#endif /* RR_1PASS_COUNT */
			"-i "
#endif /* RR_HELP_OPTS */
			"] <RR-size> <file-number>\n"
			"\t<in-file1> [ <in-file2> ... ] <in-RR-file>\n"
			"\t<out-file1> [ <out-file2> ... ] <out-RR-file> )\n"
			"\nSWITCHES (required) (NOT for old mode!):"
			"\n"
			"\n  -@, --input-list=<file>     - RR-list to process (see format below)"
			"\n                                (NOT relative to source-dir)"
			"\nNEW MODE OPTIONS:"
			"\n"
			"\n  -s, --source-dir=<path>     - directory which contains source files"
			"\n                                (usually on CD-ROM) (default: current dir)"
			"\n                                (ATTENTION: All file paths except ones"
			"\n                                that specified explicitly about are"
			"\n                                relative to source-dir)"
			"\n  -t, --target-dir=<path>     - directory for recovered files"
			"\n                                (NOT relative to source-dir"
#ifndef RR_CHECK_ALWAYS
                                                                   ";"
			"\n                                required - except with \"-c\""
#else /* RR_CHECK_ALWAYS */
                                                                   "; default: none -"
			"\n                                files will be only checked for readability"
			"\n                                and/or recoverability according to \"-a\""
//#error "убрать упоминания об -n при ndef RR_NO_RESTORE"
	#if ( defined( RR_COPY_ONLY ) || defined( RR_NO_RESTORE ) )
			                                                                          ","
			"\n                                "
		#if ( defined( RR_COPY_ONLY ) && defined( RR_NO_RESTORE ) )
			                                   "\"-p\", \"-n\""
		#elif ( defined( RR_COPY_ONLY ) )
			                                   "\"-p\""
		#elif ( defined( RR_NO_RESTORE ) )
                                         "\"-n\""
		#endif /* RR_COPY_ONLY, RR_NO_RESTORE */
	#endif /* RR_COPY_ONLY || RR_NO_RESTORE */
#endif /* RR_CHECK_ALWAYS */
			                                   ")"
#ifdef RR_UPDATE_DIR
			"\n  -u, --update-dir=<path>     - directory which contains updated versions of"
			"\n                                NON-PROTECTED files (codecs, drivers, etc.)."
			"\n                                Warning: NO version check will be performed!"
			"\n                                (NOT relative to source-dir and target-dir;"
			"\n                                makes no sense without \"-a\" or \"-r\";"
			"\n                                default: none)"
			"\n  -r, --update-protected      - allows update of PROTECTED files also (is"
			"\n                                ignored if ho \"-u\"). USE WITH CAUTION!!!"
#endif /* RR_UPDATE_DIR */
//			"\n                                (NOT relative to source-dir)"
			"\n  -o, --output-base=<name>    - output base name for m2cdm files"
			"\n                                (w/o extension, NOT relative to source-dir"
			"\n                                and target-dir, default: none)"
			"\n  -e, --form2-ext=<ext>       - form2 file extension (as for m2cdm;"
			"\n                                overrides \"-e\" line from RR-list;"
			"\n                                default: \"" RR_DEFAULT_XCD_EXT "\")"
			"\n  -l, --volume-label=<label>  - volume label (for m2cdm's \"-v\")"
			"\n                                (overrides \"-l\" line from RR-list)"
			"\n  -x, --xcd-output            - recover XCD-files as XCD instead of native"
			"\n                                (conflicts with \"-o\""
#ifdef RR_NO_RESTORE
			                                                        ", \"-n\""
#endif /* RR_NO_RESTORE */
			                                                                "; is ignored"
			"\n                                if no \"-t\")"
			"\n  -a, --all-files             - for recovering ALL (including non-protected)"
			"\n                                files from source-dir into target-dir"
//			"\n                                (is ignored if no \"-t\")"
			"\nOLD MODE OPTIONS:"
			"\n"
#ifdef RR_OLD_RRLIST
			"\n  -f, --make-rrlist           - generate RR-list"
#endif /* RR_OLD_RRLIST */
			"\n  -i, --iso-input             - form1 (ISO) input files only; otherwise,"
			"\n                                all input must be of form2 (XCD) mode"
			"\nCOMMON OPTIONS:"
			"\n"
			"\n  -h, --help                  - this help text"
			"\n  -w, --overwrite             - allow overwriting existing files in target"
			"\n                                directory (is ignored if "
#ifndef RR_OVERWRITE_OLDMODE
					"no \"-t\""
#else /* RR_OVERWRITE_OLDMODE */
					"no target specified"
#error "Доделать"
#endif /* RR_OVERWRITE_OLDMODE */
			")"
#ifdef RR_KEEP_EXISTING
			"\n  -k, --keep-existing         - keep existing files in target directory"
			"\n                                instead of overwriting them"
			"\n                                (is ignored if no \"-t\")"
#endif /* RR_KEEP_EXISTING */
#ifdef RR_COPY_ONLY
			"\n  -p, --copy-only             - only copy protected files; DO NOT recover"
			"\n                                damages, DO NOT check for correctness"
	#if ( !defined( RR_CHECK_ALWAYS ) || defined( RR_NO_RESTORE ) )
			"\n                                (conflicts with "
		#if ( !defined( RR_CHECK_ALWAYS ) && defined( RR_NO_RESTORE ) )
			                                                  "\"-n\, \"-c\""
		#elif ( !defined( RR_CHECK_ALWAYS ) )
			                                                  "\"-c\""
		#elif ( defined( RR_NO_RESTORE ) )
			                                                  "\"-n\""
		#endif /* ( !RR_CHECK_ALWAYS, RR_NO_RESTORE ) */
			                                                  ")"
	#endif /* ( !RR_CHECK_ALWAYS || RR_NO_RESTORE ) */
#endif /* RR_COPY_ONLY */
#ifdef RR_NO_RESTORE
			"\n  -n, --no-restore            - do not restore files (except mode2-form2)"
			"\n                                and do not recover damages (useful for"
			"\n                                generating m2cdm files from existing"
			"\n                                copy) "
			                                        "(requires \"-t\", implies \"-a\,"
			"\n                                conflicts with \"-x\""
	#ifndef RR_CHECK_ALWAYS
			                                                      ", \"-c\""
	#endif /* RR_CHECK_ALWAYS */
	#ifdef RR_COPY_ONLY
			                                                             ", \"-p\""
	#endif /* RR_COPY_ONLY */
			                                                                     ")"
#endif /* RR_NO_RESTORE */
#ifdef RR_1PASS_COUNT
			"\n  -f, --on-the-fly=<sectors>  - max number of sectors to recover during"
			"\n                                first phase (default: %d sectors at cost"
			"\n                                of approximately one minute of CPU time;"
			"\n                                0 means no on-the-fly recover at all)"
	#ifdef RR_NO_RESTORE
			"\n                                (conflicts with \"-n\")"
	#endif /* RR_NO_RESTORE */
#endif /* RR_1PASS_COUNT */
#if 0
//			"\n  -p, --probe[=<words>]       - probe mode: instead of recovering whole set"
//			"\n                                of sectors damaged, recover given number of"
//			"\n                                words in one (intact) sector only; useful for"
//			"\n                                quick-estimating  whether RR-protected disc"
//			"\n                                is recoverable"
//			"\n                                (default: --probe=4 - consumes ~5 seconds of CPU)"
//			"\n                                "
//			"\n                                !!! is not applied yet !!!"
			"\n  -p, --probe[=<bytes>]       - probe mode: recover given number of bytes"
			"\n                                in first non-damaged sector and compare"
			"\n                                recovered data with initial value"
			"\n                                (%d...%d bytes, default: %d bytes at the"
			"\n                                cost of few seconds of CPU time) (useful"
			"\n                                for quick-estimate data recoverability)"
			"\n                                (conflicts with \"-n\", \"-o\", \"-t\", \"-a\", \"-w\")"
			"\n  -r, --stream-read           - for probe mode: successive read input data"
			"\n                                instead of reading of needed sectors only;"
			"\n                                may speed up reading on certain CD-ROMs and"
			"\n                                slow down on others (is ignored if no \"-p\")"
#endif
#ifndef RR_CHECK_ALWAYS
			"\n  -c, --extra-check           - check for correctness of recovered files"
			"\n                                and/or proper redundancy data; also check"
			"\n                                for readability of non-protected files"
			"\n                                if \"-a\" is set"
	#if ( defined( RR_COPY_ONLY ) || defined( RR_NO_RESTORE ) )
			                                                  " (conflicts with "
		#if ( defined( RR_COPY_ONLY ) && defined( RR_NO_RESTORE ) )
																												                 "\"-n\", \"-p\""
		#elif ( defined( RR_COPY_ONLY ) )
																												                 "\"-p\""
		#elif ( defined( RR_NO_RESTORE ) )
																												                 "\"-n\""
		#endif /* RR_COPY_ONLY, RR_NO_RESTORE */
																												                       ")"
	#endif /* RR_COPY_ONLY || RR_NO_RESTORE */
#endif /* RR_CHECK_ALWAYS	*/
#ifdef RR_NO_INIT_READ
			"\n  -c, --no-check-hdr          - do NOT check whether file(s) is actually of"
			"\n                                mode2-form2 (may be useful to speed up init"
			"\n                                phase when disc being processed contains"
			"\n                                a lot of files)"
#endif /* RR_NO_INIT_READ */
			"\n  -z, --zero-damaged          - treat zero-filled sectors of ISO files as"
			"\n                                damaged (useful after CD recover by CDCHECK"
			"\n                                or similar programs)"
#ifdef RR_DMG_LIST
			"\n  -d, --damage-list=<file>    - list of damaged files and sectors"
			"\n                                (NOT relative to source-dir)"
			"\n                                (see format below)"
#endif /* RR_DMG_LIST */
#ifdef RR_KEEP_BAD
			"\n  -b, --keep-broken           - do NOT erase badly recovered files even"
			"\n                                if fatal error occurred"
#endif /* RR_KEEP_BAD */
			"\n  -v, --verbose               - tiresome mode (for debugging)"
			"\n  -q, --quiet                 - silent mode - only serious errors are"
			"\n                                displayed (for batch mode)"
//			"\n"
#ifdef RR_DMG_LIST
			"\nDAMAGE-LIST FORMAT:"
			"\n"
			"\n  -f <file>                           - file is damaged/destroyed entirely"
			"\n  -s <sector> <file>                  - file has a damaged sector"
			"\n  -r <from-sector> <to-sector> <file> - file has a range of damaged sectors"
			"\n  -b <from-byte> <to-byte> <file>     - file has a range of damaged bytes"
			"\n"
#endif /* RR_DMG_LIST */
			"\nRR-LIST FORMAT:"
			"\n"
			"\n  <size>          - first line only: RR-data (redundancy) size"
			"\n                    (in sectors)"
			"\n  # <any text>    - comment"
			"\n                  - empty line"
			"\n  -t              - strict mode (preserve size/timestamp for ALL files)"
#ifdef RR_OEM_API
			"\n  -a              - assume ANSI charset for file names (must be placed"
			"\n                    before first label or file line; default: OEM/DOS)"
#endif /* RR_OEM_API */
			"\n  -u              - multi-track XCD image (default: single-track)"
#ifdef RR_KEEP_OLD_EXT
			"\n  -k              - keep original file extension for XCD files (i.e."
			"\n                    \"movie.avi\" becomes \"movie.avi." RR_DEFAULT_XCD_EXT "\"; also breaks"
			"\n                    ISO Level 1 compatibility)"
#endif /* RR_KEEP_OLD_EXT */
#ifdef RR_CUSTOM_XCD_EXT
			"\n  -e <ext>        - extension for XCD-files; have LESS priority than"
			"\n                    \"-e\" command-line option; MUST be the first option"
			"\n                    with parameter in file"
#endif /* RR_CUSTOM_XCD_EXT */
#ifdef RR_STORE_EDC
			"\n  -g <digest>     - digest-checking mode (useful for diagnosing when "
			"\n                    redundancy check failed due to accidental changes"
			"\n                    in some iles); currently, the only digest supported"
			"\n                    is EDC (\"-g edc\")"
#endif /* RR_STORE_EDC */
			"\n  -l <label>      - volume label (for m2cdm's \"-v\"; have LESS priority"
			"\n                    than \"-l\" command-line option)"
			"\n  -c <parameter>  - extra m2cdm parameter"
//			"\n\nLines to be passed into m2cdm paramfile AS IS:"
			"\n  -d <directory>"
			"\n  --              - end-of-list"
			"\n\nFile descriptions (corresponding directives for paramfile will be"
			"\ngenerated if need):"
			"\n"	// "\nLines to be converted into m2cdm format w/o RR-processing:"
			"\n  -f <ISO-file>     - form1 (ISO) file, not protected (in non-strict mode)"
			"\n  -m <XCD-file>     - form2 (XCD) file, not protected (in non-strict mode)"
#ifdef RR_FLIST_ITSELF
			"\n  -@ <RR-list>      - RR-list itself (in non-strict mode), will be"
			"\n                      treated as \"-f <RR-list>\" for m2cdm"
			"\n  -@ <size> <timestamp> <RR-list>"
			"\n                    - same as above, but in strict mode"
#ifdef RR_LIST_2PASS
			"\n  -n <linkname>     - "
			"\n                      "
#endif /* RR_LIST_2PASS */
#endif /* RR_FLIST_ITSELF */
//			"\n\nLines to be RR-processed and converted into m2cdm format:"
			"\n  <process-type> <size> <timestamp> <filename>"
#ifdef RR_STORE_EDC
			"\nor in digest-checking mode (for RR-protected files only):"
			"\n  <process-type> <size> <timestamp> <digest> <filename>"
#endif /* RR_STORE_EDC */
			"\n\nProcess types:"
			"\n  -i      - form1 (ISO) file, RR-protected"
					// " - will be converted into \"-f <filename>\""
			"\n  -x      - form2 (XCD) file, RR-protected"
					// " - will be converted into \"-m <filename>\""
			"\n  -I, -X  - same as above but with alignment (need for last file only)"
//			"\nand also in strict mode:"
			"\n  -f      - ISO-file, not protected (in strict mode)"
					// " - convert into \"-f <filename>\""
			"\n  -m      - XCD-file, not protected (in strict mode)"
					// " - convert into \"-m <filename>\""
//			"\n\nor as above but in non-strict mode:"

#ifdef RR_DMG_ENTRIES
			"\n\nDamage descriptions (apply to previous file description, in case when"
			"\nRR-recovering copy of CD-ROM previosly restored by external program):"
			"\n  -w                           - whole file is destroyed"
			"\n  -s <sector-from> <sector-to> - file has a series of damaged sectors"
			"\n  -b <byte-from> <byte-to>     - file has a series of damaged bytes"
#endif /* RR_DMG_ENTRIES */
#ifdef RR_ERRORLEVEL
			"\n\nERRORLEVEL CODES (for use in batch files):"
			"\n"
			"\n  %d  - success; all data is recovered/recoverable, redundancy check is OK"
			"\n  %d  - too much of data damages; recovered files still have damages and/or"
			"\n       redundancy check is impossible; try to read disc in another drive"
			"\n  %d  - redundancy check failed - improper RR-data; recover is impossible"
			"\n  %d  - fatal error is occurred (most probably there was out-of-space on"
			"\n       target drive, or out-of-memory)"
			"\n  %d  - error in parameters; please check parameters list"
//			"\n  %d  - success"
#endif /* RR_ERRORLEVEL */
			"\n"
			"\nSee also:"
			"\nhttp://forum.doom9.org/showthread.php?t=93048 (english)"
			"\nhttp://forum.sharereactor.ru/showthread.php?t=59777 (russian)"
			"\n",
			retname, retname
#ifdef RR_1PASS_COUNT
			, ABS_MAX_FOR_1PASS * RR_STREAMNUM
#endif /* RR_1PASS_COUNT */
#if 0
			, PROBE_LEN_MIN, PROBE_LEN_MAX, PROBE_LEN_DEFAULT
#endif
#ifdef RR_ERRORLEVEL
			, RR_RC_OK, RR_RC_INCOMPLETE, RR_RC_UNUSABLE, RR_RC_FATAL,
			RR_RC_ILLEGAL_PARAM
#endif /* RR_ERRORLEVEL */
			);
}
opts_t global_opts;
char empty_line[] = "";
char vol_label[ 0x20 ] = "";
char xcd_ext[ 0x10 ] = "";
//char cur_base[] = ".";
const char *progname;
#ifdef RR_STORE_EDC
	LIST_HEAD( global_list );	//сразу проинициализируем
#endif /* RR_STORE_EDC */

#define SET_FILE_TYPE( type ) \
	do { \
		last_type = (type); \
	} while (0)

#define CHK_FILE_TYPE( type ) \
	do { \
		if ( ( m2cdm_file != NULL ) && ( last_type != (type) ) ) { \
			last_type = (type); \
			fprintf( m2cdm_file, "%s\n", (type) ? M2CDM_CMD_FILE : M2CDM_CMD_MOVIE ); \
		} \
	} while (0)

#define MAKE_SECLEN( type, seclen ) \
	do { \
		if ( ( seclen = rr_make_seclen( type ) ) <= 0 ) \
			goto err01; \
	} while (0)

#define PARSE_REST \
	do { \
		char *tlen, *tdate; \
		if ( ( ( tlen = strtok( rest, " \t\n" ) ) == NULL ) || \
				( ( tdate = strtok( NULL, " \t\n" ) ) == NULL ) || \
				( ( rest = strtok( NULL, "\t\n" ) ) == NULL ) || \
				( ( len = rr_atoi( tlen ) ) < -1 ) || \
				( ( date = rr_atotime( tdate ) ) < -1 ) ) \
			goto sw_err; \
	} while (0)
#ifdef RR_DMG_ENTRIES
#define PARSE2_REST \
	do { \
		char *tstart, *tend; \
		if ( ( ( tstart = strtok( rest, " \t\n" ) ) == NULL ) || \
				( ( tend = strtok( NULL, " \t\n" ) ) == NULL ) || \
				( ( start = rr_atoi( tstart ) ) < 0 ) || \
				( ( end = rr_atoi( tend ) ) < 0 ) ) \
			goto sw_err; \
	} while (0)
#endif /* RR_DMG_ENTRIES */

#ifndef RR_KEEP_BAD
	#define GLOBAL_ATTRIBS_KEEP_BAD 0
#else /* RR_KEEP_BAD */
	#define GLOBAL_ATTRIBS_KEEP_BAD \
		( global_opts.keep_bad_files ? KEEP_BAD_FILE : 0 )
#endif /* RR_KEEP_BAD */

#ifndef RR_KEEP_EXISTING
	#define GLOBAL_ATTRIBS_KEEP_EXISTING 0
#else /* RR_KEEP_EXISTING */
	#define GLOBAL_ATTRIBS_KEEP_EXISTING \
		( global_opts.keep_exist_files ? KEEP_EXIST_FILE : 0 )
#endif /* RR_KEEP_EXISTING */

#ifndef RR_NO_INIT_READ
	#define GLOBAL_ATTRIBS_NO_INIT_READ 0
	int init_noread;
#else /* RR_NO_INIT_READ */
	#define GLOBAL_ATTRIBS_NO_INIT_READ \
		( global_opts.init_noread ? NO_INIT_READ : 0 )
#endif /* RR_NO_INIT_READ */

#define GLOBAL_ATTRIBS \
	( ( global_opts.xcd_output ? NO_XCDDECODE : 0 ) | \
	( global_opts.overwrite ? ALLOW_OVERWRITE : 0 ) | \
	GLOBAL_ATTRIBS_KEEP_BAD | GLOBAL_ATTRIBS_KEEP_EXISTING | \
	GLOBAL_ATTRIBS_NO_INIT_READ )

#ifndef RR_DEC_PATH
	#define OUT_FULL_NAME rest
	#define CHECK_OUT_FULL_NAME do {} while (0)
#else /* RR_DEC_PATH */
	#ifndef RR_IGNORE_DIR
static char *rr_combo_name( char *outname, const char *base, const char *name ) {
	char t_name[ _MAX_FNAME ], t_ext[ _MAX_EXT ];
	_splitpath( name, NULL, NULL, t_name, t_ext );
	return ( rr_fullname(outname, base, t_name, t_ext, "target file", 0 ) );
}
	#endif /* RR_IGNORE_DIR */
	#define OUT_FULL_NAME out_full_name
	#define CHECK_OUT_FULL_NAME \
		do { \
			if ( ( global_opts.tgt_base != NULL ) && \
					( rr_combo_name( out_full_name, out_dir, rest ) == NULL ) ) \
				goto err01; \
		} while (0)
#endif /* RR_DEC_PATH */

#define TGT_NAME ( ( global_opts.tgt_base == NULL ) ? NULL : OUT_FULL_NAME )

#ifndef RR_ABS_NAME
	#define PARSE_ABS_NAME do {} while (0)
#else /* RR_ABS_NAME */
#error "TODO"
	#define PARSE_ABS_NAME \
		do { \
			/* если первый символ имени == ':' - считать путь абсолютным, */ \
			/* :<абс-имя-откуда>|<имя-куда> */ \
			if ( rest[ 0 ] == ':' ) { \
				t_attrib |= NO_SRCBASE; rest++; \
			} \
		} while (0)
#endif /* RR_ABS_NAME */
#ifndef RR_STORE_EDC
	#define ALL_FILES
#else /* RR_STORE_EDC */
	#define ALL_FILES global_list,
#endif /* RR_STORE_EDC */

#ifndef RR_UPDATE_DIR
	#define CHK_SRC_FILE \
		do { \
			if ( *all ) { \
				int seclen = 0; \
				MAKE_SECLEN( t_type, seclen ); \
				CHECK_OUT_FULL_NAME; \
				if ( ( item = rr_append( ALL_FILES *all, seclen, t_name, TGT_NAME, \
						len, date, t_attrib /*| NO_RRPROCESS*/ ) ) == NULL ) { \
					printf( " -- this file will not be %s\n", \
							CHECK_ONLY ? "checked" : "copied" ); \
					/* goto err01; */ \
				} \
			} \
		} while (0)
	#define GEN_M2CDM_SRC_LINE \
		do { \
			CHK_FILE_TYPE( t_type ); \
			if ( m2cdm_file != NULL ) { \
				char *t_desc, *t_base; \
				if ( is_name ) { \
					if ( global_opts.all_files ) { \
						t_base = rr_tgtbase( *all ); t_name = rest; \
					} else \
						t_base = "."; \
					t_desc = CONCAT_NAME( src, desc ); \
				} else if ( global_opts.all_files ) { \
					t_base = rr_tgtbase( *all ); t_desc = "target file"; \
				} else { \
					t_base = rr_srcbase( in ); t_desc = "source file"; \
				} \
				if ( rr_fullname( outbuf, t_base, t_name, NULL, t_desc, 1 ) == NULL ) \
					goto err01; \
				CHECK_ISO1; \
				fprintf( m2cdm_file, "%s\n", outbuf ); \
			} \
		} while (0)
#else /* RR_UPDATE_DIR */
//	#error "А на кой вообще ляд обрабатывать -f / -m без -a ?"
	#define CHK_SRC_FILE \
		do { \
			int seclen = 0; \
			MAKE_SECLEN( t_type, seclen ); \
			CHECK_OUT_FULL_NAME; \
			if ( ( item = rr_append( ALL_FILES *all, seclen, t_name, \
					global_opts.all_files ? TGT_NAME : NULL, len, date, \
					t_attrib /*| NO_RRPROCESS*/ ) ) == NULL ) { \
				if ( ( m2cdm_file != NULL ) || global_opts.all_files ) \
					rr_printmsg( VERBOSE_QUIET, " -- this file will not be %s\n", \
							( CHECK_ONLY /*|| !global_opts.all_files*/ ) \
							? "checked" : "copied" ); \
				/* goto err01; */ \
			} \
		} while (0)
	#define GEN_M2CDM_SRC_LINE \
		do { \
			if ( item != NULL ) { \
				CHK_FILE_TYPE( t_type ); \
				if ( /*( item != NULL ) &&*/ ( m2cdm_file != NULL ) ) { \
					fprintf( m2cdm_file, "%s\n", \
								global_opts.all_files ? rr_full_out_name( item ) : \
								rr_full_in_name( item ) ); \
				} \
			} \
		} while (0)
#endif /* RR_UPDATE_DIR */

#ifndef RR_LEGALZERO
int get_number( const char *inbuf ) {
	size_t jx;
	for ( jx = 0; jx < strlen( inbuf ); jx++ ) {
		if ( !isdigit( inbuf[ jx ] ) ) {
			return ( -1 );
		}
	}
	return ( atoi( inbuf ) );
}
#endif /* RR_LEGALZERO */
rrchain_t *old_rrdec_prepare( int argc, const char * const *argv,
		int *max_seclen ) {
	rrchain_t *in = NULL;
	int rrcount, infilenum, jx;
//	char outbuf[ 2 *_MAX_PATH ];
//	M2CDM_FILE_BUFFERS;

	if (
#ifndef RR_LEGALZERO
		( rrcount = get_number( argv[ 0 ] ) ) <= 0
#else /* RR_LEGALZERO */
		( rrcount = rr_atoi( argv[ 0 ] ) ) <= 0
#endif /* RR_LEGALZERO */
		) {
		rr_printmsg( VERBOSE_QUIET, "invalid redundancy length -- %s\n",
			argv[ 0 ] );
		goto err01;
	}
	if (
#ifndef RR_LEGALZERO
		( infilenum = get_number( argv[ 1 ] ) ) <= 0
#else /* RR_LEGALZERO */
		( infilenum = rr_atoi( argv[ 1 ] ) ) <= 0
#endif /* RR_LEGALZERO */
		) {
nofilenum:
		rr_printmsg( VERBOSE_QUIET, "invalid number of input files -- %s\n",
			argv[ 1 ] );
		goto err01;
	}
// добавим поддержку тестирования для старого формата:
	if ( ( argc != ( infilenum * 2 + 4 ) )
#ifdef RR_CHECK_OLDMODE
		&& ( argc != ( infilenum + 3 ) )
#error "Тут ещё надо подумать - взвести флажок, правильно передать имена..."
#endif /* RR_CHECK_OLDMODE */
		)
		goto nofilenum;
	rrcount /= RR_STREAMNUM;

// разбор командной строки:
	if ( ( in = rr_create( "RRDEC", global_opts.xcd_ext, rrcount * RR_STREAMNUM,
			NULL, NULL,
#ifdef RR_UPDATE_DIR
			NULL,
#endif /* RR_UPDATE_DIR */
#ifdef RR_ALLOW_ABSPATH
			1,	// всегда распяливать base-каталоги до абсолютных
#endif /* RR_ALLOW_ABSPATH */
			global_opts.verbose_level ) ) == NULL ) {
		goto err01;
	}

//	CREATE_M2CDM_FILE;
	global_opts.errcount = rrcount;

	for ( jx = 0; jx <= infilenum; jx++ ) {
		if ( rr_append( ALL_FILES in, global_opts.ogm_len, argv[ 2 + jx ],
#ifndef RR_CHECK_OLDMODE
				argv[ 3 + jx + infilenum ],
#else /* RR_CHECK_OLDMODE */
				global_opts.check ? argv[ 3 + jx + infilenum ] : NULL,
#error "проверить/доделать"
#endif /* RR_CHECK_OLDMODE */
				-1, -1, NEED_ALIGN | GLOBAL_ATTRIBS |
				( global_opts.zero_as_bad ? ZERO_DAMAGED : 0 )
#ifdef RR_NO_RESTORE
				| ( global_opts.no_restore ? NO_COPYPLAIN : 0 )
#endif /* RR_NO_RESTORE */
				) == NULL )
			goto err01;
	}
	*max_seclen = rr_blklen( in );
	global_opts.tgt_base = empty_line;
	goto exit;
err01:
	if ( in ) {
		rr_discard( in ); in = NULL;
	}
exit:
	return ( in );
}

#ifdef RR_DMG_ENTRIES
static int rr_markbadsegment( rritem_t *file, int is_bytes,
		int blk_from, int blk_to ) {
	int start, len;
	rrchain_t *rrchain;
	if ( file == NULL )
		return ( FATAL_ERROR );
	start = rr_get1st_blk( file );
	rrchain = rr_getchain( file );
	len = rr_getblks( file );
	if ( is_bytes &&
			( ( ( blk_from = rr_byte2blk( file, blk_from ) ) <= SOFT_ERROR ) ||
			( ( blk_to = rr_byte2blk( file, blk_to ) ) <= SOFT_ERROR ) ) )
		return ( FATAL_ERROR );

	if ( blk_from < 0 )
		blk_from = 0;
	if ( ( blk_to < 0 ) || ( blk_to >= len ) )
		blk_to = len - 1;
	return ( rr_corrupt_blks( rrchain, blk_from + start, blk_to + start ) );
}
#endif /* RR_DMG_ENTRIES */

//char
//#ifdef RR_UPDATE_DIR
//	upd_base[ _MAX_PATH ],
//#endif /* RR_UPDATE_DIR */
//	src_base[ _MAX_PATH ], tgt_base[ _MAX_PATH ];

#ifdef RR_OEM_API
#if 0
int rrdec_create( int rrcount, rrchain_t **in, rrchain_t **all,
		char *m2cdm_fname, char *m2cdm_path, char *m2cdm_buf,
		char *m2cdm_desc
								 ) {
	if ( ( in != NULL ) &&
			( ( *in = rr_create( "RRDEC", global_opts.xcd_ext, rrcount * RR_STREAMNUM,
			global_opts.src_base, global_opts.tgt_base,
#ifdef RR_UPDATE_DIR
			global_opts.upd_protected ? global_opts.upd_base : NULL,
#endif /* RR_UPDATE_DIR */
			NEED_ABSBASE
			global_opts.verbose_level ) )
			== NULL ) ) {
		return (FATAL_ERROR);
	}
#ifndef RR_UPDATE_DIR
	if ( global_opts.all_files ) {
#endif /* RR_UPDATE_DIR */
		if ( ( all != NULL ) &&
				( ( *all = rr_create( "RR-ALL", global_opts.xcd_ext, 0,
				global_opts.src_base, global_opts.tgt_base,
#ifdef RR_UPDATE_DIR
				global_opts.upd_base,
#endif /* RR_UPDATE_DIR */
				NEED_ABSBASE
				global_opts.verbose_level ) )
				== NULL ) )
		return (HARD_ERROR);
//		global_opts.src_base = global_opts.tgt_base;
#ifndef RR_UPDATE_DIR
	}
#endif /* RR_UPDATE_DIR */
	return ( NO_ERROR );
// создаём выходной файл:
	CREATE_M2CDM_FILE;
}
#else /*0*/
#ifdef RR_UPDATE_DIR
	#define RRDEC_CRE_UPDBASE \
		global_opts.upd_protected ? global_opts.upd_base : NULL,
	#define RRDEC_CRE_UPDBASE2 global_opts.upd_base,
	#define RRDEC_CRE_ALLFILES 1
#else /* RR_UPDATE_DIR */
	#define RRDEC_CRE_UPDBASE
	#define RRDEC_CRE_UPDBASE2
	#define RRDEC_CRE_ALLFILES ( global_opts.all_files )
#endif /* RR_UPDATE_DIR */

#ifndef RR_CUSTOM_XCD_EXT
	#define SET_XCD_EXT do {} while (0)
#else /* RR_CUSTOM_XCD_EXT */
	#define SET_XCD_EXT \
		do { \
			if ( global_opts.xcd_ext == NULL ) { \
				strcpy( xcd_ext, RR_DEFAULT_XCD_EXT ); \
				global_opts.xcd_ext = xcd_ext; \
			} \
		} while (0)
#endif /* RR_CUSTOM_XCD_EXT */

#define RRDEC_CREATE \
	do { \
		SET_XCD_EXT; \
		if ( ( in = rr_create( "RRDEC", global_opts.xcd_ext, rrcount * RR_STREAMNUM, \
				global_opts.src_base, global_opts.tgt_base, \
				RRDEC_CRE_UPDBASE \
				NEED_ABSBASE \
				global_opts.verbose_level ) ) \
				== NULL ) { \
			goto err01; \
		} \
		if ( RRDEC_CRE_ALLFILES ) { \
			if ( ( *all = rr_create( "RR-ALL", global_opts.xcd_ext, 0, \
					global_opts.src_base, global_opts.tgt_base, \
					RRDEC_CRE_UPDBASE2 \
					NEED_ABSBASE \
					global_opts.verbose_level ) ) \
					== NULL ) \
				goto err01; \
			/* global_opts.src_base = global_opts.tgt_base; */ \
		} \
	/* создаём выходной файл: */ \
		CREATE_M2CDM_FILE; \
	} while (0)
#endif /*0*/
#endif /* RR_OEM_API */

#define CHECK_FERRORS \
	do { \
		if ( ( m2cdm_file != NULL ) && ( ( rc = ferror( m2cdm_file ) ) != 0 ) ) { \
			rr_printmsg( VERBOSE_QUIET, \
				"%s %s: write failed: %d - %s\n", m2cdm_desc, \
				m2cdm_fname, rc, strerror( rc ) ); \
			goto err01; \
		} \
	} while (0)

rrchain_t *rrdec_prepare( rrchain_t **all, int *max_seclen ) {
	char *m2cdm_fname = global_opts.out_base;
	char *src_fname = global_opts.filelist;
	FILE *src_file = NULL, *m2cdm_file = NULL;
	int rc, rrcount, line = 0;
	rrchain_t *in = NULL;
	int last_type = -1 /*, max_seclen = 0 */;
	rritem_t *item = NULL;
#ifdef RR_DEC_PATH
	char out_dir[ _MAX_PATH ] = "", out_full_name[ _MAX_PATH ];
#endif /* RR_DEC_PATH */
//#define INBUFLEN ( _MAX_PATH + 64 )
	char inbuf[ INBUFLEN + 1 ];
#ifndef RR_UPDATE_DIR
	char outbuf[ 2 *_MAX_PATH ] = "";
#endif /* RR_UPDATE_DIR */
	M2CDM_FILE_BUFFERS;
#ifdef RR_OEM_API
	int charset_done = 0;
#endif /* RR_OEM_API */
#ifdef RR_CHECK_ISO1
	int is_iso1 = 1;
#endif /* RR_CHECK_ISO1 */
#ifdef RR_KEEP_OLD_EXT
	int is_keepext = 0;
#endif /* RR_KEEP_OLD_EXT */
	SRC_FILE_BUFFERS( "source RR-list file" );
	OPEN_SPECIAL_FILE( src, NULL, NULL );
// первая строка - rrcount:
	if ( ( rc = rrc_fgets( src_file, global_opts.filelist,
			&line, inbuf, INBUFLEN ) ) <= 0 ) {
		if ( rc < 0 ) goto err01;
		// eof:
		rr_printmsg( VERBOSE_QUIET, "%s %s: invalid file format\n",
			CONCAT_NAME( src, desc ), global_opts.filelist );
		goto err01;
	}
//#error "Добавить комментарии перед первой строкой"
	if (
#ifndef RR_LEGALZERO
		( rrcount = get_number( inbuf ) ) < 0
#else /* RR_LEGALZERO */
		( rrcount = rr_atoi( inbuf ) ) < 0
#endif /* RR_LEGALZERO */
		) {
		rr_printmsg( VERBOSE_QUIET,
			"%s %s, line %d: invalid line format\n", CONCAT_NAME( src, desc ),
			global_opts.filelist, line );
		goto err01;
	} else if ( ( rrcount <= 0 ) ||
			( rrcount > MAX_REDUNDANCY ) ) {
		rr_printmsg( VERBOSE_QUIET,
			"%s %s, line %d: invalid redundancy length\n", CONCAT_NAME( src, desc ),
			global_opts.filelist, line );
		goto err01;
	}
	rrcount /= RR_STREAMNUM;

// разбор файла:
//#error "Перенести оба rr_create() после SetFileApisToOEM()"
#ifndef RR_OEM_API
	#error "noRR_OEM_API is not supported already!"
#if 0
	if ( ( in = rr_create( "RRDEC", global_opts.xcd_ext, rrcount * RR_STREAMNUM,
			global_opts.src_base, global_opts.tgt_base,
#ifdef RR_UPDATE_DIR
			global_opts.upd_protected ? global_opts.upd_base : NULL,
#endif /* RR_UPDATE_DIR */
			NEED_ABSBASE
			global_opts.verbose_level ) )
			== NULL ) {
		goto err01;
	}
#ifndef RR_UPDATE_DIR
	if ( global_opts.all_files ) {
#endif /* RR_UPDATE_DIR */
		if ( ( *all = rr_create( "RR-ALL", global_opts.xcd_ext, 0,
				global_opts.src_base, global_opts.tgt_base,
#ifdef RR_UPDATE_DIR
				global_opts.upd_base,
#endif /* RR_UPDATE_DIR */
				NEED_ABSBASE
				global_opts.verbose_level ) )
				== NULL )
			goto err01;
//		global_opts.src_base = global_opts.tgt_base;
#ifndef RR_UPDATE_DIR
	}
#endif /* RR_UPDATE_DIR */
// создаём выходной файл:
	CREATE_M2CDM_FILE;
#else /* i.e. not 0 */
#ifndef RR_CUSTOM_XCD_EXT
	#error "noRR_CUSTOM_XCD_EXT is not supported already!"
//	RRDEC_CREATE;
#endif /* RR_CUSTOM_XCD_EXT */
#endif /*0*/
#endif /* RR_OEM_API */
	global_opts.errcount = rrcount;
	while ( ( rc = rrc_fgets( src_file, global_opts.filelist,
			&line, inbuf, INBUFLEN ) ) > 0 ) {
//		char *err_src, *err_file;
		char *first = strtok( inbuf, " \t\n" );
		if ( first == NULL ) {
			continue;
		}
		switch ( first[ 0 ] ) {
			case '#':	// комментарий - не трогаем
			case '\0':	// пустая строка - игнорируем
				continue;
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
sw_err2:
					rc = -1;
					goto err01;
				}
				rest = strtok( NULL, "\t\n" );
				if ( rest == NULL ) switch ( first[ 1 ] ) {	// ничего нет, кроме ключа
					default: {
						goto sw_err;
					} break;
					case '-': {	// конец списка
						goto eof;
					} break;
#ifdef RR_KEEP_OLD_EXT
					case 'k': {
						global_opts.keep_ext = 1;
					} break;
#endif /* RR_KEEP_OLD_EXT */
					case 'u': {
						global_opts.multitrack = 1;
					} break;
					case 't': {
						global_opts.strict_mode = 1;
					} break;
#ifdef RR_OEM_API
					case 'a': {
						if ( charset_done )
							goto sw_err;
						global_opts.ansi_api = 1;
					} break;
#endif /* RR_OEM_API */
#ifdef RR_DMG_ENTRIES
					case 'w': {	// весь файл битый
						if ( rr_markbadsegment( item, 0, 0, -1 ) < NO_ERROR ) {
							goto sw_err;
						}
					} break;
#endif /* RR_DMG_ENTRIES */
				} else {
#ifdef RR_OEM_API
					if ( !charset_done ) {
#ifdef __WIN32__
						if ( !global_opts.ansi_api ) {
							ANSI2OEM( global_opts.src_base );
							ANSI2OEM( global_opts.tgt_base );
	#ifdef RR_UPDATE_DIR
							ANSI2OEM( global_opts.upd_base );
	#endif /* RR_UPDATE_DIR */
							ANSI2OEM( global_opts.filelist );
							ANSI2OEM( global_opts.xcd_ext );
							ANSI2OEM( global_opts.volume );
							ANSI2OEM( global_opts.out_base );
	#ifdef RR_DMG_LIST
							ANSI2OEM( global_opts.damage_list );
	#endif /* RR_DMG_LIST */
							ANSI2OEM( src_fullname );
							SetFileApisToOEM();

//							if ( m2cdm_file != NULL )
//								fprintf( m2cdm_file, M2CDM_CMD_DOS "\n" );
//							SetConsoleOutputCP( 1251 );
//						} else {
//							SetConsoleOutputCP( 1251 );
						}
#endif /* __WIN32__ */
	#ifndef RR_CUSTOM_XCD_EXT
		#error "noRR_CUSTOM_XCD_EXT is not supported already!"
						RRDEC_CREATE;
	#endif /* RR_CUSTOM_XCD_EXT */
//						if ( rrdec_create( rrcount, &in, all ) < SOFT_ERROR )
//							goto err01;
						charset_done = 1;
					}
#ifdef RR_CUSTOM_XCD_EXT
					if ( ( first[ 1 ] != 'e' ) && ( in == NULL ) )
						RRDEC_CREATE;
#endif /* RR_CUSTOM_XCD_EXT */
#endif /* RR_OEM_API */
					switch ( first[ 1 ] ) {
						default: {
							goto sw_err;
						} break;
#ifdef RR_CUSTOM_XCD_EXT
						case 'e': {	// form-2 file extension
							if ( in != NULL )
								goto sw_err;
							if ( global_opts.xcd_ext == NULL ) {
								strncpy( xcd_ext, rest, sizeof( xcd_ext ) - 1 );
								xcd_ext[ sizeof( xcd_ext ) - 1 ] = 0;
								global_opts.xcd_ext = xcd_ext;
							}
						} break;
#endif /* RR_CUSTOM_XCD_EXT */
						case 'd': {	// каталог на XCD
#ifdef RR_DEC_PATH
							if ( strlen( rest ) >= _MAX_PATH )
								goto sw_err;
							strcpy( out_dir, rest );
#endif /* RR_DEC_PATH */
							SET_FILE_TYPE( -1 );
							if ( m2cdm_file != NULL )
								fprintf( m2cdm_file, M2CDM_CMD_DIR "\n%s\n", rest );
						} break;
#ifdef RR_FLIST_ITSELF
	#define RR_MAYBE_LISTFILE \
	( ( first[ 1 ] == '@' ) ? /*global_opts.filelist*/ src_fullname : NULL )
						case '@':		// list file
#else /* RR_FLIST_ITSELF */
	#define RR_MAYBE_LISTFILE NULL
#endif /* RR_FLIST_ITSELF */
						case 'm':   // form-2 file
#ifdef RR_KEEP_OLD_EXT
							is_keepext = global_opts.keep_ext;
#endif /* RR_KEEP_OLD_EXT */
						case 'f': { // form-1 file
//							PUT_SRC_FILE( first[ 1 ] != 'm', RR_MAYBE_LISTFILE );
							time_t date = -1;
							int len = -1,
								t_type = ( first[ 1 ] != 'm' ),
								t_attrib = GLOBAL_ATTRIBS;
							char *t_name = RR_MAYBE_LISTFILE; int is_name = 1;
							(void) is_name;	// нужен только GEN_M2CDM_SRC_LINE без RR_UPDATE_DIR
							if ( global_opts.strict_mode ) PARSE_REST;
							if ( t_name == NULL ) {
								PARSE_ABS_NAME;
								is_name = 0; t_name = rest;
							} else
								t_attrib |= NO_SRCBASE;
							CHK_SRC_FILE;
							/* CHK_FILE_TYPE( first[ 1 ] != 'm' ); */
							GEN_M2CDM_SRC_LINE;
#ifdef RR_NO_RESTORE
#error "обламывать -n, если есть xcd-файл в незащищённых и не задан -a"
#endif /* RR_NO_RESTORE */
						} break;
						case 'x':		// RR-protected form-2
						case 'X': 	// RR-protected form-2 aligned
#ifdef RR_KEEP_OLD_EXT
							is_keepext = global_opts.keep_ext;
#endif /* RR_KEEP_OLD_EXT */
						case 'i':		// RR-protected form-1
						case 'I': {	// RR-protected form-1 aligned
//							PUT_TGT_FILE( ( first[ 1 ] == 'i' ) || ( first[ 1 ] == 'I' ),
//								isupper( first[ 1 ] ) ? NEED_ALIGN : 0 );
							time_t date = -1;
							int len = -1, seclen = 0, \
								t_type = ( first[ 1 ] == 'i' ) || ( first[ 1 ] == 'I' )
									/*, t_align = ( isupper( first[ 1 ] ) ? NEED_ALIGN : 0 )*/;
							PARSE_REST;
							MAKE_SECLEN( t_type, seclen );
							CHK_FILE_TYPE( t_type );
							/* fprintf( rrdec_file, "%s %d %d %s\n", first, */
									/* len, date, rest ); */
							CHECK_OUT_FULL_NAME;
							if ( ( item = rr_append( ALL_FILES in, seclen, rest,
									TGT_NAME,
									len, date,
									( isupper( first[ 1 ] ) ? NEED_ALIGN : 0 ) |
									GLOBAL_ATTRIBS |
									( global_opts.zero_as_bad ? ZERO_DAMAGED : 0 )
#ifdef RR_NO_RESTORE
									| ( global_opts.no_restore ? NO_COPYPLAIN : 0 )
#endif /* RR_HO_RESTORE */
									) ) == NULL )
								goto err01;
							if ( m2cdm_file != NULL ) {
								int flags = rr_lastfile_flags( in );
								if ( flags < NO_ERROR )
									goto err01;
								CHECK_ISO1;
								/* _makepath( outbuf, NULL,
										( ( flags & ( NO_COPYPLAIN | XCD_INPUT ) ) == NO_COPYPLAIN ) ?
										rr_srcbase( in ) : rr_tgtbase( in ), rest, NULL );
								fprintf( m2cdm_file, "%s\n", outbuf ); */
								fprintf( m2cdm_file, "%s\n",
										( ( flags & ( NO_COPYPLAIN | XCD_INPUT ) ) ==
										NO_COPYPLAIN ) ?
										rr_full_in_name( item ) : rr_full_out_name( item ) );
							}
						} break;
						case 'l': {	// volume label
							if ( global_opts.volume == NULL ) {
								strncpy( vol_label, rest, sizeof( vol_label ) - 1 );
								vol_label[ sizeof( vol_label ) - 1 ] = 0;
								global_opts.volume = vol_label;
								SET_FILE_TYPE( -1 );
								if ( m2cdm_file != NULL )
									fprintf( m2cdm_file, M2CDM_CMD_VOLUME "\n%s\n", rest );
							}
						} break;
						case 'c': {	// явный параметр для m2cdm
							SET_FILE_TYPE( -1 );
							if ( m2cdm_file != NULL )
								fprintf( m2cdm_file, "%s\n", rest );
						} break;
#ifdef RR_DMG_ENTRIES
						case 's': {	// битые секторы от-до
							int start, end;
							PARSE2_REST;
							if ( rr_markbadsegment( item, 0, start, end ) < NO_ERROR )
								goto sw_err;
						} break;
						case 'b': {	// битые байты от-до
							int start, end;
							PARSE2_REST;
							if ( rr_markbadsegment( item, 1, start, end ) < NO_ERROR )
								goto sw_err;
						} break;
#endif /* RR_DMG_ENTRIES */
					}
				}
			} break;
		}
#ifndef NDEBUG
		if ( m2cdm_file != NULL )
			fflush( m2cdm_file );
#endif /* NDEBUG */
		CHECK_FERRORS;
	}
eof:
	if ( rc < 0 )	// прочиталось из списка что-то не то либо слишком много
		goto sw_err2;
#ifdef RR_CHECK_ISO1
	if ( m2cdm_file != NULL ) {
		if ( is_iso1
#ifdef RR_KEEP_OLD_EXT
				&& !is_keepext
#endif /* RR_KEEP_OLD_EXT */
			) {
			fprintf( m2cdm_file, M2CDM_CMD_ISO1 "\n" );
			CHECK_FERRORS;
		} else
			rr_printmsg( VERBOSE_NORMAL,
				"Warning: Some files are NOT compatible with ISO Level 1\n" );
	}
#endif /* RR_CHECK_ISO1 */
	*max_seclen = rr_blklen( in );
	if (
#ifdef RR_UPDATE_DIR
			global_opts.all_files &&
#endif /* RR_UPDATE_DIR */
			( *all != NULL ) ) {
		int all_len = rr_blklen( *all );
		if ( all_len > *max_seclen )
			*max_seclen = all_len;
	}

	//	rr_print( in );
	rc = 0; goto exit;
err01:
	rc = 1;
	if ( in ) {
		rr_discard( in ); in = NULL;
	}
	if ( *all ) {
		rr_discard( *all ); *all = NULL;
	}
exit:
	if ( src_file != NULL )
		fclose( src_file );
	if ( m2cdm_file != NULL )
		fclose( m2cdm_file );
	return ( in );
}

int rr_thorough_read( rrchain_t *rrchain, char *buf, int bufblks ) {
/* -------------------------------------------- *
 * проход первый с половиной - если много сбоев *
 * -------------------------------------------- */
	int jx, jy, readiness = 0, readerrs, errcount;
	if ( bufblks > XCD_BUFBLKS ) bufblks = XCD_BUFBLKS;
//		int longbad = 0;
	rr_setbufsize( rrchain, 1 );
	rr_seek( rrchain, 0 ); jx = 0;
	while ( ( jy = rr_untouched( rrchain, jx ) ) >= 0 ) {
		int jr, jl = rr_typelen( rrchain, jy, bufblks );
//		if ( jl > bufblks ) jl = bufblks;
		rr_seek( rrchain, jy );
//			printf( "read = %d; reserve = %d; pos = %d; len = %d\n",
//					jr, reserve, jy, jl );
		jr = rr_read( rrchain, buf, jl, &readerrs
#ifdef RR_SHOW_LOST
				, NULL
#endif /* RR_SHOW_LOST */
				);
		if ( jr <= FATAL_ERROR )
			return ( jr );
		if ( jr < NO_ERROR )
#ifndef RR_FILL_BAD
			jr = 1;
#else /* RR_FILL_BAD */
			jr = 0;
#endif /* RR_FILL_BAD */
//		jl += jr; jx = jy + jl;
		jx = jy + jr;
		errcount = rr_errcount( rrchain );
		readiness = rr_readiness( rrchain );
		rr_printmsg( VERBOSE_QUIET,
				LONGBLANKLINE "%2d.%01d%% verified, %d errors ",
				readiness / 10, readiness % 10, errcount );
		if ( rr_used_rrcount( rrchain, 1 ) <= global_opts.errcount )
			break;
	}
	rr_printmsg( VERBOSE_QUIET, "\n" );
	return ( rr_used_rrcount( rrchain, 1 ) );
}
#ifndef RR_NEW_ENC
static int rr_readchain( rrchain_t *rrchain, char *buf,
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
				"" );
		if ( ( error_count != NULL ) && ( ( buf_errs
#ifdef RR_SHOW_LOST
				+ buf_lost
#endif /* RR_SHOW_LOST */
				) > 0 ) )
			rr_printmsg( VERBOSE_NORMAL, "\n" );
	}
	return ( total_blks );
}

static int rr_check_result( rrchain_t *rrchain, rrtable_t *rr, char *buf,
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
		rr_printmsg( VERBOSE_NORMAL, "Writing repaired sectors...\n" );
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
#endif /* RR_NEW_ENC */
int main( int argc, char **argv ) {
	int pass_num = 0;
	rrchain_t *in = NULL;
//	int out = -1;
	int rc;
	int
		rrcount, bufblks, buflen;
	int total_blks;
	char *buf = NULL;
	char **errlocs;
	int error_count, jx;
	rrtable_t *rr = NULL;
//	int reserve; //precision_flag;
	rrchain_t *all = NULL;
//	char timebuf[ 0x100 ] = "";
//	char default_ext[ sizeof( RR_DEFAULT_XCD_EXT ) + 1 ];

#define PROBE_LEN global_opts.probe_len
#define PROBE_LEN_ROUNDED ( ( global_opts.probe_len + 7 ) & ~7 )
	int max_seclen;
	int no_2pass = 0;
//	char *decoy_buf;
//	int decoy_len, decoy_seclen;
	rritem_t *decoy = NULL;

	progname = argv[ 0 ];
#ifdef __WIN32__
	SetConsoleTitle( RR_SOFTNAME " (" RR_SOFTDESC ") v" RR_SOFTVER );
#endif /* __WIN32__ */
//	copyright();
	memset( &global_opts, 0, sizeof( global_opts ) );
	global_opts.program = PROGRAM_RRDEC;
	global_opts.ogm_len = XCD_DATA_LEN;
	global_opts.verbose_level = VERBOSE_NORMAL;
#ifdef RR_1PASS_COUNT
	global_opts.max_for_1pass = ABS_MAX_FOR_1PASS;
#endif /* RR_1PASS_COUNT */
/* ------------------------- *
 * Анализ командной строки   *
 * --------------------------*/
//	global_opts.version = -1;
	if ( ( jx = rr_parse_args( argc, argv, long_opts ) ) < 0 ) {	// кривые аргументы
//wrong_cmdline:
		copyright();
		rr_printmsg( VERBOSE_QUIET, "Illegal command line" );
wrong_param:
		rr_printmsg( VERBOSE_QUIET, " -- use -h for help.\n\n" );
//		usage();
no_todo:
		return ( RR_RC_ILLEGAL_PARAM );
	}
//	jx--;
	if ( global_opts.verbose_level > VERBOSE_NORMAL )
		copyright();
	argv += jx; argc -= jx;
	if ( argc > 0 ) {	// old compat mode
		global_opts.old_compat_mode = 1;
		rr_printmsg( VERBOSE_INFO, "Old version compatibility mode enabled\n" );
		if ( global_opts.version[ 1 ] != 0 ) {
wrong_version:
			copyright();
			rr_printmsg( VERBOSE_QUIET, "Illegal compatibility modes option mix is set" );
			goto wrong_param;
		}
	} else {	// новая версия
// проверим параметры:
		if ( global_opts.version[ 0 ] != 0 )
			goto wrong_version;
		if ( global_opts.filelist == NULL /* || ( global_opts.errcount < 0 ) */
				/* || ( global_opts.out_base == NULL ) */ ) {
#ifndef RR_CHECK_ALWAYS
no_req_opt:
#endif /* RR_CHECK_ALWAYS */
			copyright();
			rr_printmsg( VERBOSE_QUIET, "Required option is not set" );
			goto wrong_param;
		}
	}

/* ------------------------------------ *
 * Проверка параметров на совместимость *
 * ------------------------------------ */
// %%%%%% сделать как-то попроще
#ifndef RR_CHECK_ALWAYS
	if ( ( global_opts.tgt_base == NULL ) && !global_opts.check ) {
		goto no_req_opt;
	}
#endif /* RR_CHECK_ALWAYS */
	if ( 0
#ifdef RR_NO_RESTORE
			|| ( global_opts.no_restore && ( global_opts.all_files
	#ifndef RR_CHECK_ALWAYS
			|| global_opts.check
	#endif /* RR_CHECK_ALWAYS */
			|| global_opts.xcd_output
//#ifdef RR_COPY_ONLY
//			|| global_opts.copy_only
//#endif /* RR_COPY_ONLY */
			) )
#endif /* RR_NO_RESTORE */
#ifdef RR_COPY_ONLY
	#ifndef RR_CHECK_ALWAYS
			|| ( global_opts.copy_only && global_opts.check )
	#endif /* RR_CHECK_ALWAYS */
#endif /* RR_COPY_ONLY */
			|| ( global_opts.xcd_output && ( global_opts.out_base != NULL ) )
			) {
		copyright();
		rr_printmsg( VERBOSE_QUIET, "Mutually exclusive options are set" );
		goto wrong_param;
	}
#ifdef RR_CHECK_ALWAYS
	global_opts.check = !( 0
	#ifdef RR_NO_RESTORE
			|| global_opts.no_restore
	#endif /* RR_NO_RESTORE */
	#ifdef RR_COPY_ONLY
			|| global_opts.copy_only
	#endif /* RR_COPY_ONLY */
			);
#endif /* RR_CHECK_ALWAYS */

	if ( ( global_opts.tgt_base == NULL ) && global_opts.check ) {
#ifndef RR_CHECK_ALWAYS
		global_opts.checkonly = 1;
#endif /* RR_CHECK_ALWAYS */
//		if ( global_opts.all_files ) {
//			rr_printmsg( VERBOSE_NORMAL, "Option \"--all-files\" is ignored.\n" );
//			global_opts.all_files = 0;
//		}
		if ( global_opts.overwrite ) {
			rr_printmsg( VERBOSE_INFO, "Option \"--overwrite\" is ignored.\n" );
			global_opts.overwrite = 0;
		}
	}
	if ( global_opts.src_base == NULL )
		global_opts.src_base = empty_line;
#ifndef RR_BASE_EXT
	if ( rr_test_outbase() )
		goto wrong_param;
#endif /* RR_BASE_EXT */

// умолчания:
	if (
#ifndef RR_CUSTOM_XCD_EXT
			global_opts.old_compat_mode &&
#endif /* RR_CUSTOM_XCD_EXT */
			global_opts.xcd_ext == NULL ) {
		strcpy( xcd_ext, RR_DEFAULT_XCD_EXT );
		global_opts.xcd_ext = xcd_ext;
	}

/* ----------------------------- *
 * обработка списковых файлов    *
 * ----------------------------- */
	rr_printmsg( VERBOSE_INFO, "Pass 0: analyzing and opening files\n" );

	if ( ( in = ( global_opts.old_compat_mode ?
			old_rrdec_prepare( argc, (const char * const *) argv, &max_seclen ) :
			rrdec_prepare( &all, &max_seclen ) ) ) == NULL ) {
		goto fatal;
	}
	rr_printmsg( VERBOSE_NORMAL, "Recoverable bad sectors limit: %d\n",
		global_opts.errcount * RR_STREAMNUM );
/* ----------------------------- *
 * подготовка к обработке данных *
 * ----------------------------- */
	rr_printmsg( VERBOSE_INFO, "\n" );
	if ( ( all != NULL ) && ( rr_files( all ) == 0 ) ) {
		rr_discard( all ); all = NULL;
	}
	if ( ( rr_files( in ) == 0 ) && ( all == NULL ) ) {
		rr_printmsg( VERBOSE_QUIET, "Nothing to process; exiting.\n" );
		rr_discard( in ); in = NULL;
		goto no_todo;
	}
	rrcount = global_opts.errcount;
	bufblks = BLKNUM;
	if ( rrcount > bufblks )
		bufblks = rrcount;
	buflen = bufblks * ( max_seclen * RR_STREAMNUM );

	if ( ( buf = malloc(
			buflen + rrcount * RR_STREAMNUM * sizeof( void* ) ) ) == NULL ) {
mem_low:
		rr_printmsg( VERBOSE_QUIET, "Insufficient memory available\n" );
		goto fatal;
	}
	errlocs = (char **) ( buf + buflen );
//	printf( "rr_init() ready. Press any key " ); _getch();
/* --------------------------------------------------- *
 * Копирование незащищённых файлов (если заказывалось) *
 * --------------------------------------------------- */
	if (
#ifdef RR_UPDATE_DIR
			global_opts.all_files &&
#endif /* RR_UPDATE_DIR */
			( all != NULL ) ) {
		OGM_LEN = rr_blklen( all );
		rr_printmsg( VERBOSE_QUIET,
				"Pass %d: %s non-protected files\n", ++pass_num,
				CHECK_ONLY ? ( LOOK_ONLY ? "Looking at" : "Checking" ) : "Copying" );
		rr_print( all );
		if ( ( rc = rr_readchain( all, buf, bufblks,
				/*XCD_BUFBLKS*/ bufblks * RR_STREAMNUM, NULL, &error_count ) )
				< NO_ERROR ) {
			if ( ( rc <= FATAL_ERROR ) && !CHECK_ONLY ) {
//				rr_printmsg( VERBOSE_INFO, "Discarding non-protected files\n" );
				goto fatal;
			}
		}
	}
	if ( all != NULL ) {
		rr_close( all );
		all = NULL;
		rr_printmsg( VERBOSE_QUIET, "\n" ); // \nRecovering protected files..." );
	}
	if ( rr_files( in ) == 0 ) {	// защищённых нет
		goto restored_ok;
	}
/* ------------------------------------------------------------- *
 * Первый проход: копирование (если надо), проверка (если можно) *
 * ------------------------------------------------------------- */
	OGM_LEN = rr_blklen( in );
	rr_printmsg( VERBOSE_QUIET, "Pass %d: %s protected files\n", ++pass_num,
			CHECK_ONLY ? ( LOOK_ONLY ? "Looking at" : "Checking" ) : "Copying" );
	rr_print( in );	// публикация списка обрабатываемых файлов
// а вообще - возможно ли?
#ifndef RR_CHECK_ALWAYS
	if ( ( rrcount = rr_used_rrcount( in, 0 ) ) > ( global_opts.errcount -
			( global_opts.check ? 1 : 0 ) ) ) {	// увы, восстановление невозможно
//		assert( !"в rr_append не вылетать, если rrcount исчерпан; "
//				"на второй проход не ходить" );
		no_2pass = 1;
//		if ( !global_opts.no_restore ) {
		rr_printmsg( VERBOSE_QUIET,
				"Sorry... %s is impossible because too much data lost!\n",
				global_opts.checkonly ? "Extra check" : "Complete recover" );
//#error "и перед вторым проходом тоже проверить"
//			goto err;
//		}
//		global_opts.check = /*global_opts.checkonly =*/ 0;
	} else if ( !( global_opts.no_restore
	#ifdef RR_COPY_ONLY
			|| global_opts.copy_only
	#endif /* RR_COPY_ONLY */
			) ) {
		if ( global_opts.check ) {	// готовим контрольный блок
			if ( ( decoy = rr_add_decoy( in, RR_STREAMNUM, 0 ) ) == NULL ) {
				rr_printmsg( VERBOSE_QUIET,
						"%s: Unable to initialize extra check\n",
						global_opts.checkonly ? "Error" : "Warning" );
			} else {
				rrcount++;
			}
		}
	#ifdef RR_1PASS_COUNT
		if ( global_opts.check && ( global_opts.max_for_1pass > 0 ) )
			global_opts.max_for_1pass++;
		if ( global_opts.max_for_1pass > global_opts.errcount )
			global_opts.max_for_1pass = global_opts.errcount;
	#endif /* RR_1PASS_COUNT */
//		rrcount = rr_used_rrcount( in, 0 ) + RESERVE_FOR_1PASS;
		if ( MAX_FOR_1PASS > 0 ) {
			if ( rrcount > MAX_FOR_1PASS ) {
				rr_printmsg( VERBOSE_INFO, "%s will be postponed until next pass\n",
						global_opts.checkonly ? "Extra check" : "Complete recover" );
			} else {
				if ( ( rr = rr_init( MAX_FOR_1PASS, OGM_LEN * RR_STREAMNUM ) ) == NULL )
					goto mem_low;
				if ( decoy != NULL )
					rr_printmsg( VERBOSE_INFO, "Extra check activated\n" );
			}
		}
	}
#error "ndef RR_CHECK_ALWAYS уже не поддерживается"
#else /* RR_CHECK_ALWAYS */
	if ( global_opts.check ) {
		if ( ( rrcount = rr_used_rrcount( in, 0 ) ) > global_opts.errcount ) {
			no_2pass = 1;
			rr_printmsg( VERBOSE_QUIET,
					"Sorry... %s is impossible because too much data lost!\n",
				CHECK_ONLY ? "Extra check" : "Complete recover" );
		} else
	#if ( defined( RR_NO_RESTORE ) || defined( RR_COPY_ONLY ) )
			if ( !( 0
		#ifdef RR_NO_RESTORE
				|| global_opts.no_restore
		#endif /* RR_NO_RESTORE */
		#ifdef RR_COPY_ONLY
				|| global_opts.copy_only
		#endif /* RR_COPY_ONLY */
				) )
	#endif /* defined( RR_NO_RESTORE ) || defined( RR_COPY_ONLY ) */
				{
			if ( ( decoy = rr_add_decoy( in, RR_STREAMNUM, 0 ) ) == NULL ) {
				rr_printmsg( VERBOSE_QUIET,
						"%s: Unable to initialize extra check\n",
						CHECK_ONLY ? "Error" : "Warning" );
			} else {
				rrcount++;
			}
	#ifdef RR_1PASS_COUNT
			if ( global_opts.max_for_1pass > 0 )
				global_opts.max_for_1pass++;
			if ( global_opts.max_for_1pass > global_opts.errcount )
				global_opts.max_for_1pass = global_opts.errcount;
	#endif /* RR_1PASS_COUNT */
	//		rrcount = rr_used_rrcount( in, 0 ) + RESERVE_FOR_1PASS;
			if ( MAX_FOR_1PASS > 0 ) {
				if ( rrcount > MAX_FOR_1PASS ) {
					rr_printmsg( VERBOSE_INFO, "%s will be postponed until next pass\n",
							CHECK_ONLY ? "Extra check" : "Complete recover" );
				} else {
					if ( ( rr = rr_init( MAX_FOR_1PASS, OGM_LEN * RR_STREAMNUM ) ) == NULL )
						goto mem_low;
					if ( decoy != NULL )
						rr_printmsg( VERBOSE_INFO, "Extra check activated\n" );
				}
			}
		}
	} else {
		no_2pass = 1;
	}
#endif /* RR_CHECK_ALWAYS */
	if ( ( rc = rr_readchain( in, buf, bufblks,
			/*XCD_BUFBLKS*/ bufblks * RR_STREAMNUM, rr, &error_count ) )
			< NO_ERROR )
		goto prot_discard;
/* ------------------------------ *
 * Анализ (после первого прохода) *
 * ------------------------------ */
	rrcount = rr_used_rrcount( in, 1 );
	if ( no_2pass
#ifdef RR_NO_RESTORE
			|| global_opts.no_restore
#endif /* RR_NO_RESTORE */
#ifdef RR_COPY_ONLY
			|| global_opts.copy_only
#endif /* RR_COPY_ONLY */
			) {
//		assert( !"Разобраться с rc!" );
		if ( rrcount > ( /*( decoy != NULL ) ? 1 :*/ 0 ) ) {
			rr_printmsg( VERBOSE_QUIET,
#ifdef RR_CHECK_ALWAYS
					"\nWarning: there were errors %s\n",
					CHECK_ONLY ? "during check" : "in recovered files!"
#else /* RR_CHECK_ALWAYS */
					"\nWarning: there were errors in recovered files!\n"
#endif /* RR_CHECK_ALWAYS */
					);
			goto not_restored;
		}
		goto restored_ok;
	}
/*
	if ( ( rrcount <= MAX_FOR_1PASS ) && ( rr != NULL ) ) {	// вписались -
		goto check_result;	// можно восстанавливать/проверять
	} else
*/
//#error "Продолжить здесь"
	if ( /* ( MAX_FOR_1PASS > 0 ) && */
			( ( rrcount > MAX_FOR_1PASS ) || ( rr == NULL ) ) ) {
		if ( rr != NULL ) {	// всё равно наново пересчитывать
			rr_cleanup( rr ); rr = NULL;
		}
		if ( rrcount > global_opts.errcount ) {
			if ( !CHECK_ONLY ) {
//				assert( !"Ошибок больше нормы - промежуточный проход" );
				rr_printmsg( VERBOSE_QUIET,
						"\nPass %d: There are too many errors; "
						"re-reading rough sectors\n", ++pass_num );
				if ( ( rrcount = rr_thorough_read( in, buf, bufblks ) ) < NO_ERROR ) {
//					if ( rc <= FATAL_ERROR )
						goto prot_discard;
				}
			}
			if ( rrcount > global_opts.errcount ) {
//many_errs:
				rr_printmsg( VERBOSE_QUIET, "Sorry... Still too many errors!\n"
						"Try to read this disk in another CD-ROM drive.\n" );
//				assert( !"Разобраться с rc!" );
				goto not_restored;
//			goto abort;
			}
		}
		/* -------------------------------------------------------- *
		 * второй проход - собственно проверка и/или восстановление *
		 * -------------------------------------------------------- */
//		assert( !"Второй проход -- rrcount должен быть актуальным!" );
		rr_printmsg( VERBOSE_QUIET,
				"\nPass %d: Performing calculations\n", ++pass_num );
		if ( ( rr = rr_init( rrcount, OGM_LEN * RR_STREAMNUM ) ) == NULL )
			goto mem_low;
		if ( decoy != NULL )
			rr_printmsg( VERBOSE_INFO, "Extra check activated\n" );

		if ( rr_closein( in ) == 0 )
			rr_printmsg( VERBOSE_NORMAL, "All input files closed.\n" );
		if ( ( total_blks = rr_readchain( in, buf, bufblks,
				bufblks * RR_STREAMNUM, rr, NULL ) ) < NO_ERROR ) {
//			if ( rc <= FATAL_ERROR )
			goto prot_discard;
		}
	}
/*	rr_printmsg( VERBOSE_NORMAL, "\nPass %d: %s...\n",
			( global_opts.checkonly ||
			( global_opts.check && ( errcount == RR_STREAMNUM ) ) )
			? "Calculating" : "Recovering lost sectors", ++pass_num );
 */
	if ( ( rc = rr_check_result( in, rr, buf, errlocs, decoy ) )
			< NO_ERROR ) {
		if ( rc == SOFT_ERROR ) {	// check failed
//			if ( decoy != NULL )
//				rr_printmsg( VERBOSE_QUIET, "Extra check FAILED - files are UNUSABLE!\n" );
			rc = RR_RC_UNUSABLE;
		} else if ( rc == HARD_ERROR ) {
//many_errs:
			rr_printmsg( VERBOSE_QUIET,
					"\nUnfortunately, you have not enough recovery data\n"
					"to recover all of the lost sectors\n" );
not_restored:
			rc = RR_RC_INCOMPLETE;
		} else /*if ( rc <= FATAL_ERROR )*/ {
prot_discard:
fatal:
			rr_printmsg( VERBOSE_QUIET,
				"\nExecution terminated due to fatal error.\n" );
			if (
#ifdef RR_KEEP_BAD
					!global_opts.keep_bad_files &&
#endif /* RR_KEEP_BAD */
					!CHECK_ONLY && ( ( in != NULL ) || ( all != NULL ) ) ) {
				rr_printmsg( VERBOSE_INFO, "Discarding partially recovered files\n" );
			}
			rc = RR_RC_FATAL;
//discard:
			if ( in != NULL ) {
				global_opts.keep_bad_files ? rr_close( in ) : rr_discard( in );
				in = NULL;
			}
			if ( all != NULL ) {
				global_opts.keep_bad_files ? rr_close( all ) : rr_discard( all );
				all = NULL;
			}
		}
//#error "Здесь остановимся"
//		assert( !"Разобраться с rc!" );
//		goto exit;
	} else {	// rr_check_result >= NO_ERROR
restored_ok:
		rr_printmsg( VERBOSE_QUIET, "\nReady.\n" );
		rc = RR_RC_OK;
	}
	if ( rr != NULL )
		rr_cleanup( rr );
	if ( buf != NULL )
		free( buf );
	if ( in != NULL )
		rr_close( in );
	if ( all != NULL )
		rr_close( all );
//	if ( out != -1 )
//		_close( out );
	return ( rc );
}
