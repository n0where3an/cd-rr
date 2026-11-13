/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#ifdef __WIN32__
#include <io.h>
#else
	#include <sys/io.h>
#endif /*__WIN32__*/
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __WIN32__
#include <sys/utime.h>
#include <direct.h>
#else
    #include <sys/time.h>
    #include <unistd.h>
#endif /*__WIN32__*/
#ifdef __GCC__
#include <stdint.h>
#endif

#include "rr.h"
#include "list.h"

#include "rrfile.h"

#undef RR_DECODETEST
//#error "Протестировать по сети на живой дискете (с выдёргиванием)"
#define RR_ALLOW_TRUNC					/* !!! DANGEROUS !!! */
#define RR_CUT_COPY							/* обрезка копий при закрытии оригиналов */
#define RR_RECOVER_DELETED			/* восстановление файла, если его нет */
#undef RR_ONLY_IF_NEED_RESTORE	/* восстанавливать удалённый,   */ \
																/* только когда он явно заказан */
#define RR_OPEN_NONPROTECTED		/* пытаться восстановить невосстановимое */
#ifdef RR_LIST_2PASS
//#define RR_PERSISTENT_NAMES			/* имена файлов не копировать - вполне */
																/* хватит указателей */
#endif /* RR_LIST_2PASS */

/*#define RR_DYN_FILENAMES*/				/* динамическое выделение памяти */ \
																/* под имена файлов */
#undef RR_OLD_BLOCK							/* старый алгоритм выбора длины блока */
//#define XCD_LEN 2352
//#define OGM_LEN 2324
#undef OGM_LEN
//int ogm_len = XCD_DATA_LEN;

#pragma pack( 1 )
//#include "riffhdrs.h"
typedef enum {
	FATAL				= 0x0001,	// сбой в копии - продолжать нет смысла?
	NOTREADABLE	= 0x0002,	// прочесть неоткуда - можно только вычислить1`
	BAD					= 0x0004,	// сбой в оригинале - не читать
	SHADY				= 0x0008,	// возможно, сбойный - читать при крайней нужде
	ROUGH				= 0x0010,	// ненадёжный (EDC не совпал); можно потом перечитать
	ZEROFILLED	= 0x0020,	// сплошные нули - возможно, попытка восстановления CDCHECK'ом
	UNTOUCHED		= 0x0040,	// ещё не читанный
	DUMMY				=	0x0080,	// для выравнивания. всегда с нулями, всегда надёжный
	MISSING     = 0x0100,	// отсутствующий (или с несовпавшей длиной) файл
	GOOD				= 0x0400,	// нормально прочитался, можно брать из копии
	GENERATED   = 0x0800,	// рассчитываемый. eof при чтении, дозапись
	DECOY_BLK		= 0x1000	// обманка. при чтении сбойный. при записи сохраняет в буфере.
} rrtype_t;
#define HAS_ERRORS ( FATAL | NOTREADABLE | BAD | SHADY | ROUGH | \
		MISSING | ZEROFILLED | DECOY_BLK )
#define NOTCOPIED  ( HAS_ERRORS | UNTOUCHED )
#define COPIED ( GOOD ) /* %%% разобраться: DUMMY - тоже COPIED или нет? */
#ifndef __GCC__
#pragma warning( push )
#pragma warning( disable: 4201 4200 )
#endif
// Элемент карты секторов. Раньше это был union с uint64_t sector, через который
// элементы копировались/сравнивались целиком; на 64-битных платформах указатель
// сам занимает 8 байт, и поле type при таком копировании терялось.
typedef struct {
	rritem_t *rritem;
	rrtype_t type/*:12*/;
//	int rfile:20;
} rrmap_t;
#define RRMAP_EQ( a, b ) ( ( (a).rritem == (b).rritem ) && ( (a).type == (b).type ) )
#pragma pack()

struct rrchain_s {
	char rrname[ RRNAME_LEN + 1 ];	// имя RR-объекта
	size_t max_blklen;		// макс. длина блока (сектора) по всем rritem'ам
	int total_blks;		// общее кол-во блоков в объекте
	long total_bytes;	// --"------"-- байт ---"----"--
	int pos_blks;			// текущая позиция в объекте
	int last_error;				// последний код ошибки
	int total_files;	// общее кол-во файлов
	int err_limit;		// предельное кол-во сбойных секторов (величина избыточности)
										// > 0 только для rrdec, для остальных режимов == 0
//	int err_count;		// кол-во накопленных сбойных секторов
	int lost_blks;		// кол-во секторов в потерянных файлах
//	rrpurpose_t purpose;	// назначение rrchain'а
	int read_blks;		// текущая макс. длина буфера чтения
// для записи ремонтных данных:
#ifndef RR_NEW_ENC
	rritem_t *rrheader;
#endif /* RR_NEW_ENC */
// для обманки (check):
	rritem_t *decoy;
	int rrcount;
	verbose_t verbosity;		// уровень болтливости
// чтобы быстрее рассчитывалось кол-во секторов с ошибками:
	int
	bad_sectors,				// == -1 - изменилось, надо перечитать
	untouched_sectors;	// аналогично
	rritem_t *active_rritem;	// или NULL, если ни один файл не открыт
	int active_in_handle;
	int active_out_handle;
//	int *err_locs;
	char
	xcd_base[ _MAX_EXT ],		// типовое расширение для XCD-файлов
	src_base[ _MAX_PATH ],	// базовый путь для исходных файлов
#ifdef RR_UPDATE_DIR
	upd_base[ _MAX_PATH ],	// базовый путь для модифицированных файлов
#endif /* RR_UPDATE_DIR */
	tgt_base[ _MAX_PATH ];	// базовый путь для файлов-результатов
	struct list_head items_list;
#ifndef RR_NEW_ENC
	rrmap_t blk_map[ MAX_BLKNUM ];	// карта соответствия секторов файлам
#else /* RR_NEW_ENC */
	rrmap_t *blk_map;	// карта соответствия секторов файлам
#endif /* RR_NEW_ENC */
	char xcd_buf[ XCD_BUFBLKS * XCD_LEN ];	// %%% потом переделаю на malloc()
//	rritem_t files[ 0 ];	// список файлов
	int err_locs[ 0 ];	// номера сбойных секторов
};
struct rritem_s {
	struct list_head list_item;
	rrchain_t *rrchain;
#ifdef RR_DMG_ENTRIES
//	rrmap_t *item_map;	// первый блок rrmap'а, посвящённый этому файлу
#endif /* RR_DMG_ENTRIES */
//	int itemnum;
	int blk_from;			// блок, на который отображается начало файла
	int len_blks;			// длина файла в блоках
	size_t in_skip_bytes;		// длина заголовка файла
//	size_t in_skip_per_blk;	// длина заголовка сектора
	int need_flags;			// NEED_ALIGN | NO_SRCBASE | NO_RRPROCESS | ....
	size_t len_bytes;		// длина реального файла в байтах
	size_t native_blklen;	// длина блока для не-XCD-файла (2048 либо 2324 байта)
	size_t in_blklen;				// длина блока
//	blktype_t blktype;
//	int error;				// код последней ошибки
//	int pad_blks;			// выравнивание в начале файла (пригодится?)
//	int pos_bytes;		// позиция после последнего чтения
	size_t out_blklen;		// длина блока
	int out_blks;			// записано блоков
	size_t out_skip_bytes;		// длина заголовка файла
//	size_t out_skip_per_blk;	// длина заголовка сектора
//	long out_len_bytes;		// (требуемая) длина реального файла в байтах
	time_t filetime;
	/*size_t*/ size_t filesize;
//	int expand_blks;		// запас на дозапись RR-данных в файл
	const char *out_name;
	const char *in_name;
	const char *native_name;
	char *decoy_buf;
	int has_data;
	int read_allow;
	int write_allow;
	int recover_lost;
	const char *full_in_name;
	const char *full_out_name;
#ifdef RR_TRACK_PATH
//	ritem_t *prev_path, *next_path;
	struct list_head path_item;
#error "Дописать"
#endif /* RR_TRACK_PATH */
#ifdef RR_STORE_EDC
	struct list_head global_list;
	unsigned int
//		*edc_ptr,
	edc_array[ 0 ];
#else /* RR_STORE_EDC */
	#ifndef RR_PERSISTENT_NAMES
	char names[ 0 ];
	#endif /* RR_PERSISTENT_NAMES */
#endif /* RR_STORE_EDC */
};
#ifndef __GCC__
#pragma warning( pop )
#endif
#define XCD_START 44
const unsigned char XCD_SYNC_FIELD[ XCD_SYNC_LEN ] = {
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0
};
const riffhdr_t riffhdr = {
	{ 'R', 'I', 'F', 'F' },
	0,
	{ 'C', 'D', 'X', 'A' },
	{ 'f', 'm', 't', ' ' },
	0x10, 0, 0, 0x41585515, 1, 0, 0,
	{ 'd', 'a', 't', 'a' },
	0
};

//#error "Проверить все случаи rr_fullname на предмет - когда какой do_full"
char *rr_fullname( char *outname, const char *base, const char *name,
//#error "Разобраться с ext"
const char *ext, const char *type, int do_full ) {
	char tmppath[ _MAX_PATH * 2 ];
	int is_err;
	if ( ( base == NULL ) && ( name == NULL ) ) {
		outname[ 0 ] = '\0';
	} else {
		_makepath( tmppath, NULL, base, name, /*NULL*/ ext );
		if ( do_full )
#ifdef __WIN32__
			is_err = ( _fullpath( outname, tmppath, _MAX_PATH ) == NULL );
#else
		is_err = ( realpath(tmppath,outname) == NULL );
#endif /*__WIN32__*/
		else if ( !( is_err = ( strlen( tmppath ) >= _MAX_PATH ) ) )
			strcpy( outname, tmppath );
		if ( is_err ) {	// слишком длинное
			//			is_err = errno; //for debuging
			if (errno == ENOENT) {// && errno == ENOTDIR
				strcpy( outname, tmppath );
			} else {
				rr_printmsg( VERBOSE_QUIET,
					"\n%s %s: invalid %s%s\n", type, /*name*/ tmppath,
					do_full ? "full " : "", ( name == NULL ) ? "path" : "name" );
				return ( NULL );
			}
		}
	}
	return ( outname );
}

/*
int rr_cmppath( const rrchain_t *rrchain, const char *name, char *outname,
		const char *type, char *printf_msg, int *error ) {
	return ( 0 );
}
*/
#ifndef RR_KEEP_OLD_EXT
	#define INFILE_XCD2
#else /* RR_KEEP_OLD_EXT */
	#define INFILE_XCD2 infile_xcd2,
#endif /* RR_KEEP_OLD_EXT */
#define GEN_FULL_NAME( do_xcd ) \
	do { \
		if ( need_flags & NO_SRCBASE ) { \
			fullsrc = infile; \
		} else { \
			if ( ( fullsrc = rr_fullname( fullsrcbuf, t_base, infile, NULL, \
					"source file", 1 ) ) == NULL ) \
				goto err2; \
		} \
		if ( do_xcd ) \
			in_is_xcd = rr_isxcd( infile, infile_xcd, INFILE_XCD2 rrchain->xcd_base ); \
	} while (0)
const static unsigned long EDC_crctable[ 256 ] = {
	0x00000000l, 0x90910101l, 0x91210201l, 0x01b00300l,
	0x92410401l, 0x02d00500l, 0x03600600l, 0x93f10701l,
	0x94810801l, 0x04100900l, 0x05a00a00l, 0x95310b01l,
	0x06c00c00l, 0x96510d01l, 0x97e10e01l, 0x07700f00l,
	0x99011001l, 0x09901100l, 0x08201200l, 0x98b11301l,
	0x0b401400l, 0x9bd11501l, 0x9a611601l, 0x0af01700l,
	0x0d801800l, 0x9d111901l, 0x9ca11a01l, 0x0c301b00l,
	0x9fc11c01l, 0x0f501d00l, 0x0ee01e00l, 0x9e711f01l,
	0x82012001l, 0x12902100l, 0x13202200l, 0x83b12301l,
	0x10402400l, 0x80d12501l, 0x81612601l, 0x11f02700l,
	0x16802800l, 0x86112901l, 0x87a12a01l, 0x17302b00l,
	0x84c12c01l, 0x14502d00l, 0x15e02e00l, 0x85712f01l,
	0x1b003000l, 0x8b913101l, 0x8a213201l, 0x1ab03300l,
	0x89413401l, 0x19d03500l, 0x18603600l, 0x88f13701l,
	0x8f813801l, 0x1f103900l, 0x1ea03a00l, 0x8e313b01l,
	0x1dc03c00l, 0x8d513d01l, 0x8ce13e01l, 0x1c703f00l,
	0xb4014001l, 0x24904100l, 0x25204200l, 0xb5b14301l,
	0x26404400l, 0xb6d14501l, 0xb7614601l, 0x27f04700l,
	0x20804800l, 0xb0114901l, 0xb1a14a01l, 0x21304b00l,
	0xb2c14c01l, 0x22504d00l, 0x23e04e00l, 0xb3714f01l,
	0x2d005000l, 0xbd915101l, 0xbc215201l, 0x2cb05300l,
	0xbf415401l, 0x2fd05500l, 0x2e605600l, 0xbef15701l,
	0xb9815801l, 0x29105900l, 0x28a05a00l, 0xb8315b01l,
	0x2bc05c00l, 0xbb515d01l, 0xbae15e01l, 0x2a705f00l,
	0x36006000l, 0xa6916101l, 0xa7216201l, 0x37b06300l,
	0xa4416401l, 0x34d06500l, 0x35606600l, 0xa5f16701l,
	0xa2816801l, 0x32106900l, 0x33a06a00l, 0xa3316b01l,
	0x30c06c00l, 0xa0516d01l, 0xa1e16e01l, 0x31706f00l,
	0xaf017001l, 0x3f907100l, 0x3e207200l, 0xaeb17301l,
	0x3d407400l, 0xadd17501l, 0xac617601l, 0x3cf07700l,
	0x3b807800l, 0xab117901l, 0xaaa17a01l, 0x3a307b00l,
	0xa9c17c01l, 0x39507d00l, 0x38e07e00l, 0xa8717f01l,
	0xd8018001l, 0x48908100l, 0x49208200l, 0xd9b18301l,
	0x4a408400l, 0xdad18501l, 0xdb618601l, 0x4bf08700l,
	0x4c808800l, 0xdc118901l, 0xdda18a01l, 0x4d308b00l,
	0xdec18c01l, 0x4e508d00l, 0x4fe08e00l, 0xdf718f01l,
	0x41009000l, 0xd1919101l, 0xd0219201l, 0x40b09300l,
	0xd3419401l, 0x43d09500l, 0x42609600l, 0xd2f19701l,
	0xd5819801l, 0x45109900l, 0x44a09a00l, 0xd4319b01l,
	0x47c09c00l, 0xd7519d01l, 0xd6e19e01l, 0x46709f00l,
	0x5a00a000l, 0xca91a101l, 0xcb21a201l, 0x5bb0a300l,
	0xc841a401l, 0x58d0a500l, 0x5960a600l, 0xc9f1a701l,
	0xce81a801l, 0x5e10a900l, 0x5fa0aa00l, 0xcf31ab01l,
	0x5cc0ac00l, 0xcc51ad01l, 0xcde1ae01l, 0x5d70af00l,
	0xc301b001l, 0x5390b100l, 0x5220b200l, 0xc2b1b301l,
	0x5140b400l, 0xc1d1b501l, 0xc061b601l, 0x50f0b700l,
	0x5780b800l, 0xc711b901l, 0xc6a1ba01l, 0x5630bb00l,
	0xc5c1bc01l, 0x5550bd00l, 0x54e0be00l, 0xc471bf01l,
	0x6c00c000l, 0xfc91c101l, 0xfd21c201l, 0x6db0c300l,
	0xfe41c401l, 0x6ed0c500l, 0x6f60c600l, 0xfff1c701l,
	0xf881c801l, 0x6810c900l, 0x69a0ca00l, 0xf931cb01l,
	0x6ac0cc00l, 0xfa51cd01l, 0xfbe1ce01l, 0x6b70cf00l,
	0xf501d001l, 0x6590d100l, 0x6420d200l, 0xf4b1d301l,
	0x6740d400l, 0xf7d1d501l, 0xf661d601l, 0x66f0d700l,
	0x6180d800l, 0xf111d901l, 0xf0a1da01l, 0x6030db00l,
	0xf3c1dc01l, 0x6350dd00l, 0x62e0de00l, 0xf271df01l,
	0xee01e001l, 0x7e90e100l, 0x7f20e200l, 0xefb1e301l,
	0x7c40e400l, 0xecd1e501l, 0xed61e601l, 0x7df0e700l,
	0x7a80e800l, 0xea11e901l, 0xeba1ea01l, 0x7b30eb00l,
	0xe8c1ec01l, 0x7850ed00l, 0x79e0ee00l, 0xe971ef01l,
	0x7700f000l, 0xe791f101l, 0xe621f201l, 0x76b0f300l,
	0xe541f401l, 0x75d0f500l, 0x7460f600l, 0xe4f1f701l,
	0xe381f801l, 0x7310f900l, 0x72a0fa00l, 0xe231fb01l,
	0x71c0fc00l, 0xe151fd01l, 0xe0e1fe01l, 0x7070ff00l
};

/*static*/ unsigned long build_edc( const void *in,
unsigned from, unsigned upto ) {
	const unsigned char *p = (unsigned char *) in + from;
	unsigned long register result = 0;

	for ( ; from < upto; from++ ) {
		result = EDC_crctable[ ( result ^ *p++ ) & 0xffL ] ^ ( result >> 8 );
	}
	return result;
}
//#define _read read
//#define _eof eof

static int rr_isxcd( const char *fullname, char *xcdname,
#ifdef RR_KEEP_OLD_EXT
	char *xcdname2,
#endif /* RR_KEEP_OLD_EXT */
const char *xcdext ) {
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT], xcdext2[_MAX_EXT];
	char *ext_up;
	_splitpath( fullname, drive, dir, fname, ext );
#ifdef __WIN32__
	ext_up = strlwr( ext );
#else
	ext_up =ext;
#endif
	if ( xcdext != NULL ) {
#ifdef __WIN32__
		_makepath( xcdext2, NULL, NULL, NULL, xcdext );
#else /* __WIN32__ */
		// POSIX-версии: _splitpath() даёт расширение без точки,
		// а _makepath() без имени файла возвращает пустую строку
		strncpy( xcdext2, ( xcdext[ 0 ] == '.' ) ? xcdext + 1 : xcdext, _MAX_EXT - 1 );
		xcdext2[ _MAX_EXT - 1 ] = '\0';
#endif /* __WIN32__ */
		if ( xcdname != NULL ) {
			_makepath( xcdname, drive, dir, fname, xcdext );
		}
#ifdef RR_KEEP_OLD_EXT
		if ( xcdname2 != NULL ) {
			char fname2[ _MAX_FNAME + _MAX_EXT ];
			_makepath( fname2, NULL, NULL, fname, ext );
			_makepath( xcdname2, drive, dir, fname2, xcdext );
		}
#endif /* RR_KEEP_OLD_EXT */
	}
	if ( ext[ 0 ] == '\0' )	// файл без расширения - не XCD
		return ( 0 );
	return ( ( ( xcdext != NULL ) && ( strcmp( ext, xcdext2 ) == 0 ) ) ||
			/* ( strcmp( ext_up, ".dat" ) == 0 ) || */
		( strcmp( ext_up, RR_DEFAULT_XCD_EXT ) == 0 ) );
}

rrchain_t *rr_create( const char *rrname, const char *xcd_ext,
	int errcount, const char *src_base, const char *tgt_base,
#ifdef RR_UPDATE_DIR
	const char *upd_base,
#endif /* RR_UPDATE_DIR */
#ifndef RR_ALLOW_ABSPATH
	#define DO_ABS_BASE( base ) 1
#else /* RR_ALLOW_ABSPATH */
	int do_full,	// делать/не делать преобразование базовых путей до абсолютных
#define DO_ABS_BASE( base ) \
	( do_full || ( ( (base) != NULL ) && ( (base)[0] != '\0' ) ) )
#endif /* RR_ALLOW_ABSPATH */
verbose_t verbose ) {
	rrchain_t *rrchain;
	int jx;
	int rrsize = sizeof( rrchain_t ) // + fcount * sizeof( rritem_t )
		+ ( errcount + 1 ) * sizeof( int );
#ifdef RR_ALLOW_ABSPATH
//#error "Разобраться: upd_base передаваться не должна, а берётся за текущую"
//	if ( src_base == NULL ) src_base = "";
//	if ( tgt_base == NULL ) tgt_base = "";
	#ifdef RR_UPDATE_DIR
//	if ( upd_base == NULL ) upd_base = "";
	#endif /* RR_UPDATE_DIR */
#endif /* RR_ALLOW_ABSPATH */
	if ( rrname == NULL )
		rrname = "(unnamed)";
	if ( errcount < 0 ) {
		rr_printmsg( VERBOSE_QUIET, "RR-chain %s: error in parameters\n", rrname );
		goto err1;
	}
	if ( ( rrchain = malloc( rrsize ) ) == NULL ) {
		rr_printmsg( VERBOSE_QUIET,
			"RR-chain %s: not enough memory to get %d bytes\n", rrname, rrsize );
		goto err1;
	}
	memset( rrchain, 0, rrsize );
	strncpy( rrchain->rrname, rrname, RRNAME_LEN );
	INIT_LIST_HEAD( &rrchain->items_list );
	for ( jx = 0; jx < MAX_BLKNUM; jx++ ) {
//		rrchain->blk_map[ jx ].rfile = -1;
		rrchain->blk_map[ jx ].type = DUMMY;
		rrchain->blk_map[ jx ].rritem = NULL;
	}
	rrchain->total_blks = 0;
	rrchain->read_blks = XCD_BUFBLKS;

	rrchain->total_files = /* fcount */ 0;
	rrchain->pos_blks = 0;
	rrchain->last_error = 0;
//	rrchain->err_count = 0;
	rrchain->err_limit = errcount;
	rrchain->max_blklen = 0;
	rrchain->verbosity = verbose;
	strncpy( rrchain->xcd_base, xcd_ext, _MAX_EXT - 1 );

	for ( jx = 0; jx <= errcount; jx++ ) {
		rrchain->err_locs[ jx ] = -1;
	}
	rrchain->active_rritem = NULL;
	rrchain->active_in_handle = -1;
	rrchain->active_out_handle = -1;
	if ( ( rr_fullname( rrchain->src_base, src_base, NULL, NULL,
				"source dir", DO_ABS_BASE( src_base ) ) == NULL )
		|| ( rr_fullname( rrchain->tgt_base, tgt_base, NULL, NULL,
				"target dir", DO_ABS_BASE( tgt_base ) ) == NULL )
#ifdef RR_UPDATE_DIR
		|| ( rr_fullname( rrchain->upd_base, upd_base, NULL, NULL,
				"update dir", DO_ABS_BASE( upd_base ) ) == NULL )
#endif /* RR_UPDATE_DIR */
		)
		goto err2;
//ok:
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	return ( rrchain );
err2:
	free( rrchain );
err1:
	return ( NULL );
}
int rrh_write( int handle, char *buf, int len ) {
	int outlen = 0, was_zero = 0;
	errno = 0;
	do {
		int rc = _write( handle, buf, len );
		if ( ( rc < 0 ) || ( ( rc < len ) && ( errno != 0 ) ) )
			return ( -errno );
		if ( rc == 0 ) {
			if ( was_zero )
				return ( -EILSEQ );
			was_zero = 1;
		}
		buf += rc; len -= rc; outlen += rc;
	} while ( len > 0 );
	return ( outlen );
}

static int rr_chksize( rrchain_t *rrchain, int extra ) {
	if ( /* rr_chklen( rrchain, pad_blks ) */
		( rrchain->total_blks + extra ) > MAX_BLKNUM ) {
		rr_printmsg( VERBOSE_QUIET,
			"RR-chain %s: maximum chain size exceeded\n", rrchain->rrname );
		return ( FATAL_ERROR );
	}
	return ( NO_ERROR );
}
/*
int rr_adddummy( rrchain_t *rrchain, int blks, int need_align_head ) {
	int rc;
	int pad_blks = need_align_head ? ( ( RR_STREAMNUM - 1 ) -
		( ( rrchain->total_blks + ( RR_STREAMNUM - 1 ) ) % RR_STREAMNUM ) ) : 0;
	pad_blks += blks;
	if ( ( rc = rr_chksize( rrchain, pad_blks ) ) != NO_ERROR )
		return ( rc );
	rrchain->total_blks += pad_blks;
	return ( NO_ERROR );
}
*/
rritem_t * /*int*/ rr_setrrdata( rrchain_t *rrchain, const char *prefixfile,
const char *rrdatafile, int rrcount, int flags ) {
	int rc; rritem_t *rrfile = NULL;
#ifndef RR_NEW_ENC
	if ( rrchain->rrheader ) {	// уже есть
//		if ( rrchain->verbosity >= VERBOSE_NORMAL )
		rr_printmsg( VERBOSE_QUIET,
			"RR-chain %s: RR-header is already defined\n", rrchain->rrname );
		return ( NULL /*SOFT_ERROR*/ );
	}
#else /* RR_NEW_ENC */
	int jx, jy;
	rrmap_t sector;
#error "Нужен двухпроходный алгоритм"
#endif /* RR_NEW_ENC */
	if ( ( rrfile = rr_append( rrchain, rr_blklen( rrchain ), prefixfile,
					rrdatafile, -1, -1, flags | NEED_ALIGN | NO_SRCBASE ) ) == NULL )	// не добавился?
		return ( NULL /*FATAL_ERROR*/ );
	if ( ( rc = rr_chksize( rrchain, rrcount ) ) != NO_ERROR )
		return ( /*rc*/ NULL );
// quick hack: подделаем длину шапочного файла
//	rrfile->expand_blks = rrchain->err_limit;
	rrfile->filesize = rrfile->native_blklen *
		( rrfile->len_blks + rrcount );
// время тоже подделаем:
	rrfile->filetime = /* -1 */ time( NULL );
#ifndef RR_NEW_ENC
	rrchain->rrheader = rrfile;
#else /* RR_NEW_ENC */
	jy = rrfile->blk_from + rrfile->len_blks;
	sector.type = GENERATED;
	sector.rritem = rrfile;
	rrchain->total_blks += rrchain->rrcount;
	rrfile->len_blks += rrchain->rrcount;
	for ( jx = jy; jx < rrchain->total_blks; jx++ ) {
		rrchain->blk_map[ jx ] = sector;
	}
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
#endif /* RR_NEW_ENC */
	rrchain->rrcount = rrcount;
	return ( rrfile /*NO_ERROR*/ );
}
#ifndef RR_NEW_ENC
int rr_putrrdata( rrchain_t *rrchain, char *buf ) {
	rritem_t *file = rrchain->rrheader;
	int rc = 0, total = 0, jx, jy = file->blk_from + file->len_blks;

	rrmap_t sector;
//	sector.rfile = file->itemnum;
	sector.type = GENERATED;
	sector.rritem = file;
	rrchain->total_blks += rrchain->rrcount;
	file->len_blks += rrchain->rrcount;
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	for ( jx = jy; jx < rrchain->total_blks; jx++ ) {
		rrchain->blk_map[ jx ] = sector;
		rr_seek( rrchain, jx );
		if ( ( rc = rr_write( rrchain, buf, 1 ) ) != 1 ) {
			return ( FATAL_ERROR );
		}
		total++;
		buf += rrchain->max_blklen;
	}
	return ( total );
}
#endif /* RR_NEW_ENC */

#define DUMMY_FILE_PARAMS \
	do { \
		exactfiletype = 1; \
		in_handle = -1; \
		file->len_bytes = filelen; \
		file->len_blks = ( filelen + seclen - 1 ) / seclen; \
		/*file->in_skip_per_blk =*/ file->in_skip_bytes = 0; \
		file->in_blklen = file->native_blklen = seclen; \
		file->filesize = filelen; file->filetime = filetime; \
		sector.type = /*NOTREADABLE*/ MISSING; \
		rrchain->lost_blks += file->len_blks; \
	} while (0)

#define RRF_MSG_LEN ( _MAX_PATH + 0x400 )	// размер буфера printf_msg для rrf_open()
static int rrf_open( const char *filename, int attrib, int need_flags,
const char *type, char *printf_msg, int *error, int *fmode ) {
	int handle, rc;
	if ( ( attrib & _O_CREAT ) == _O_CREAT ) {
		handle = open( filename, _O_BINARY | _O_RANDOM | attrib |
				( ( need_flags & ALLOW_OVERWRITE ) ? _O_TRUNC : _O_EXCL ),
				_S_IREAD | _S_IWRITE | ( ( fmode != NULL && *fmode != -1) ? *fmode : 0 ) );
	} else {
		handle = open( filename, _O_BINARY | _O_RANDOM | attrib );
	}
	if ( handle == -1 ) {
		rc = errno;
		if ( error != NULL )
			*error = errno;
		if ( printf_msg != NULL )
			// имя - не длиннее _MAX_PATH, остальное помещается в 0x400
			snprintf( printf_msg, RRF_MSG_LEN, "%s file %.*s: %s failed: %d - %s", type,
				(int) _MAX_PATH, filename,
				( ( attrib & _O_CREAT ) == _O_CREAT ) ? "create" : "open",
				rc, strerror( rc ) );
	}
	if ( ( fmode != NULL ) && ( ( attrib & _O_CREAT ) != _O_CREAT ) ) {
                // читаем file_stat существующего файла
		struct stat file_stat;
		rc = fstat (handle, &file_stat);
		*fmode = (rc == 0 ? file_stat.st_mode : -1 );
	}
	return ( handle );
}

static int rr_select(		// выбор+открытие файла в цепочке
	rrchain_t *rrchain,	// цепочка
	rritem_t *file,			// описатель файла
	int *error					// код ошибки (errno)
) {
	char outbuf[ RRF_MSG_LEN ];
	int in_handle = -1, out_handle = -1;
	int rc = NO_ERROR;
	if ( rrchain->active_rritem == file/*->itemnum*/ )	// ничего не меняется
		return ( NO_ERROR );

// сначала позакрываем старые:
	if ( rrchain->active_in_handle != -1 ) {
		close( rrchain->active_in_handle );
		rrchain->active_in_handle = -1;
	}
	if ( rrchain->active_out_handle != -1 ) {
		close( rrchain->active_out_handle );
		rrchain->active_out_handle = -1;
	}
	rrchain->active_rritem = /*-1*/NULL;
	if ( file == NULL )	// просто отключить/закрыть
		return ( NO_ERROR );

// открываем входной:
	if ( ( file->read_allow /*full_in_name[ 0 ] != '\0'*/ )	&&
		( ( in_handle = rrf_open( file->full_in_name, _O_RDONLY,
						file->need_flags, "source", outbuf, error, NULL ) ) == -1 ) ) {
		int jx;
		rrmap_t *map = rrchain->blk_map + file->blk_from;
		file->read_allow = 0;
//		assert( !"правильно отметить как неоткрытый! (SOFT_ERROR)" );
		for ( jx = 0; jx < file->len_blks; jx++ ) {
			if ( map->type & ( UNTOUCHED | SHADY | ROUGH ) )
				map->type = MISSING;
			map++;
		}
		rc = SOFT_ERROR;
//		goto err1;
	}
// и выходной тоже:
	if ( ( file->write_allow /*full_out_name[ 0 ] != '\0'*/ )	&&
		( ( out_handle = rrf_open( file->full_out_name, _O_RDWR,
						file->need_flags, "target", outbuf, error, NULL ) ) == -1 ) ) {
		if ( in_handle != -1 )
			close( in_handle );
//err1:
		rrchain->bad_sectors = rrchain->untouched_sectors = -1;
		rr_printmsg( VERBOSE_QUIET, "%s\n", outbuf );
		return ( rc );
	}
	rrchain->active_in_handle = in_handle;
	rrchain->active_out_handle = out_handle;
	rrchain->active_rritem = file/*->itemnum*/;
	return ( rc );
}

#define CHECK_SECLEN \
	do { \
		if ( ( ( seclen == 0 ) && ( rrchain->total_files == 0 ) ) || \
				( seclen < 0 ) ) { \
			rr_printmsg( VERBOSE_QUIET, \
					"RR-chain %s (%d files): illegal file sector length\n", \
					rrchain->rrname, rrchain->total_files ); \
			goto err1; \
		} \
		if ( seclen == 0 ) seclen = rrchain->max_blklen; \
	} while (0)

char *rr_decoy_buf( rrchain_t *rrchain ) {
	return ( ( rrchain->decoy == NULL ) ? NULL : rrchain->decoy->decoy_buf );
}
int rr_decoy_len( rrchain_t *rrchain ) {
	return ( ( rrchain->decoy == NULL ) ? FATAL_ERROR : rrchain->decoy->len_bytes );
}
rritem_t *rr_add_decoy( rrchain_t *rrchain, int blknum, int seclen ) {
	rritem_t *file = NULL;
	int filelen, jx,
// фиктивные - для CHECK_SECLEN:
	exactfiletype, in_handle;
	time_t filetime = -1;
	rrmap_t sector;
	(void) exactfiletype; (void) in_handle;	// пишутся в DUMMY_FILE_PARAMS, здесь не нужны

	CHECK_SECLEN;
	filelen = seclen * blknum;
	if ( rr_chksize( rrchain, blknum ) < NO_ERROR )
		goto err1;

	if ( ( file = malloc( (size_t) filelen + sizeof( rritem_t ) ) ) == NULL ) {
		rr_printmsg( VERBOSE_QUIET,
			"RR-chain %s: not enough memory to get %d bytes for decoy\n",
			rrchain->rrname, filelen + (int) sizeof( rritem_t ) );
		goto err1;
	}
	file->has_data = file->recover_lost = 0;
	file->decoy_buf = (void *) ( file + 1 );
	memset( file->decoy_buf, 0, filelen );
	file->blk_from = rrchain->total_blks;
	DUMMY_FILE_PARAMS;
	sector.rritem = file; sector.type = DECOY_BLK;
//	file->filesize = filelen;
//	file->filetime = -1;
	file->full_in_name = file->full_out_name = file->native_name =
		file->in_name = file->out_name = NULL;
//	file->in_blklen = file->native_blklen = seclen;
//	file->len_blks = blknum;
	file->rrchain = rrchain;
	file->need_flags = DECOY_ITEM;
//	file->in_skip_bytes = file->in_skip_per_blk =
	file->out_blks = 0;
	file->out_blklen = file->out_skip_bytes = /*file->out_skip_per_blk =*/ 0;
	file->read_allow = file->write_allow = 0;

//	list_add_tail( &file->list_item, &rrchain->items_list );
	sector.rritem = file;
	list_add_tail( &file->list_item, &rrchain->items_list );
	if ( rrchain->max_blklen < file->native_blklen )
		rrchain->max_blklen = file->native_blklen;
	rrchain->total_files++;
	rrchain->total_bytes += file->len_bytes;
	rrchain->total_blks += file->len_blks;

	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	for ( jx = file->blk_from; jx < rrchain->total_blks; jx++ ) {
		rrchain->blk_map[ jx ] = sector;
	}
	rrchain->decoy = file;
err1:
	return ( file );
}

#ifndef RR_UPDATE_DIR
	#define RR_FILETYPE "source"
	#define RR_FILETYPE_MSG( line )	RR_FILETYPE line
#else /* RR_UPDATE_DIR */
	#define RR_FILETYPE \
		( t_base == rrchain->upd_base ) ? "update" : "source"
	#define RR_FILETYPE_MSG( line )	"%s" line, RR_FILETYPE
#endif /* RR_UPDATE_DIR */

rritem_t *rr_append(		// добавление файла в rr-цепочку
#ifdef RR_STORE_EDC
	struct list_head *all_files,
#endif /* RR_STORE_EDC */
	rrchain_t *rrchain,	// цепочка
	int seclen,						// длина сектора; == 0 - макс. длина в rchain'е
	const char *infile,		// имя входного файла (NULL - для спец.случаев)
	const char *outfile,	// имя выходного файла (NULL, если вых. файл не нужен)
	int filelen,	// длина файла (для восстановления из XCD) или -1 - не трогать
	time_t filetime,	// время модификации
int need_flags ) {	// надо ли выравнивать файл хвостом по границе кластера;
												// надо ли игнорировать source base;

	int pad_blks, jy;
	int rc;
	riffhdr_t riffbuf;
	rrmap_t sector;
	char fullsrcbuf[ _MAX_PATH * 2 ], infile_xcd[ _MAX_PATH ];
	// имена цели: на них ссылается file->full_out_name до копирования в COPY_NAME,
	// поэтому они должны жить до конца функции, а не только внутри if ( outfile )
	char native_tgt[ _MAX_PATH * 2 ], xcd_tgt[ _MAX_PATH * 2 ];
#ifdef RR_KEEP_OLD_EXT
	char infile_xcd2[ _MAX_PATH ];
#endif /* RR_KEEP_OLD_EXT */
	const char *fullsrc;
	int in_is_xcd;
	int in_handle, out_handle;
	char printf_msg[ RRF_MSG_LEN ];
	rritem_t local_file, *file = &local_file;
	int rr_filesize;
	int fmode = -1;	// права исходного файла; -1, если он не открывался
#ifndef RR_PERSISTENT_NAMES
	const char *dyn_tname;
#endif /* RR_PERSISTENT_NAMES */
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	if ( ( outfile != NULL ) && ( outfile[ 0 ] == '\0' ) )
		outfile = NULL;
	CHECK_SECLEN;
//	memset( file, 0, sizeof( rritem_t ) );
	file->decoy_buf = NULL; file->has_data = file->recover_lost = 0;

	/*file->*/out_handle = -1;
//	file->itemnum = rrchain->total_files;
	file->native_blklen = seclen;
//	file->expand_blks = 0;
//	rrchain->total_files = jx;
	sector.type = UNTOUCHED;
	file->read_allow = file->write_allow = 0;
	file->full_in_name = file->full_out_name = NULL;
	if ( infile == NULL ) {
		in_handle = -1;
		file->len_blks = file->len_bytes =
			/*file->in_skip_per_blk =*/ file->in_skip_bytes = 0;
		file->in_blklen = file->native_blklen = seclen;
		file->filesize = (size_t) -1; file->filetime = -1;
		fullsrc = "(unnamed)";
		file->in_name = file->native_name = NULL;
		in_is_xcd = 0;
	} else {
		off_t len; char *t_base;
//		int in_is_xcd;
		struct _stat l_stat;

		/*unsigned*/ int pos = XCD_START;
		int exactfiletype = 1;
		file->native_name = infile;
#ifdef RR_UPDATE_DIR
	#ifndef RR_IGNORE_DIR
		t_base = ( rrchain->upd_base[ 0 ] != (char) '\0' )
			? rrchain->upd_base : rrchain->src_base;
	#else /* RR_IGNORE_DIR */
		t_base = ( need_flags & RRLIST_ITSELF )
			? rrchain->tgt_base : ( ( rrchain->upd_base[ 0 ] != (char) '\0' )
				? rrchain->upd_base : rrchain->src_base );
	#endif /* RR_IGNORE_DIR */
	upd_retry:
		infile = file->native_name;
#else /* RR_UPDATE_DIR */
		t_base = rrchain->src_base;
#endif /* RR_UPDATE_DIR */
		in_is_xcd = 0;
	xcd_retry:
		GEN_FULL_NAME( 1 );
/* ------------------------- */
		if (_stat( fullsrc, &l_stat ) != 0 ) {
			rc = errno;
			if ( rc == ENOENT ) {	// нема файла
				if ( ( in_is_xcd == 0 ) && ( seclen == XCD_DATA_LEN ) ) {	// не берётся - поищем XCD-файл
#ifdef RR_UPDATE_DIR
					if ( t_base != rrchain->upd_base )
#endif /* RR_UPDATE_DIR */
#ifndef RR_KEEP_OLD_EXT
						rr_printmsg( VERBOSE_INFO, "source file %s: trying %s...\n",
							infile, infile_xcd );
#else /* RR_KEEP_OLD_EXT */
					rr_printmsg( VERBOSE_INFO, "source file %s not found -\n  trying %s...\n",
						infile, infile_xcd2 );
					infile = infile_xcd2;	// пробуем "name.ext.xcd"
					GEN_FULL_NAME( 0 );
					if (_stat( fullsrc, &l_stat ) == 0 )
						goto found;
					if ( ( rc = errno ) != ENOENT )
						goto bad_stat;
					rr_printmsg( VERBOSE_INFO, "  trying %s...\n", infile_xcd );
#endif /* RR_KEEP_OLD_EXT */
					infile = infile_xcd;
#ifdef RR_VCD_AS_ISO
					in_is_xcd = 1;
#endif /* RR_VCD_AS_ISO */
					goto xcd_retry;
#ifdef RR_UPDATE_DIR
				} else if ( t_base == rrchain->upd_base ) {
				upd_abort:
					t_base = rrchain->src_base;
					goto upd_retry;
#endif /* RR_UPDATE_DIR */
				}
			}
		bad_stat:
			infile = file->native_name;
			GEN_FULL_NAME( 1 );
			sprintf( printf_msg,
				RR_FILETYPE_MSG( " file %s: unable to get stat: %d - %s" ),
				fullsrc, rc, strerror( rc ) );
//			goto err3;
			goto recover;
		}
	found:
		if ( ( l_stat.st_mode & _S_IFMT ) != _S_IFREG ) {
			sprintf( printf_msg,
				RR_FILETYPE_MSG( " file %s: is not a regular file" ),
				fullsrc );
//			goto err3;
			goto recover;
		}
/* ------------------------- */
#ifdef RR_EMPTY_DELD
	#ifndef RR_UPDATE_DIR
		#error "RR_EMPTY_DELD не работает без RR_UPDATE_DIR"
	#endif /* RR_UPDATE_DIR */
		if ( ( l_stat.st_size == 0 ) && ( t_base == rrchain->upd_base ) ) {
			sprintf( printf_msg,
				RR_FILETYPE_MSG( " file %s: has zero length - treat as absent" ),
				fullsrc );
			goto forced_recover;
		}
#endif /* RR_EMPTY_DELD */
#ifdef RR_NO_INIT_READ
		if ( need_flags & NO_INIT_READ ) {
			in_handle = -1;
			goto no_open;
		}
#endif /* RR_NO_INIT_READ */
		in_handle = rrf_open( fullsrc, _O_RDONLY, need_flags,
				RR_FILETYPE, printf_msg, NULL, &fmode );
		if ( in_handle == -1 ) {
		recover:
#ifdef RR_UPDATE_DIR
			if ( t_base == rrchain->upd_base ) {	// с update-dir номер не прошёл
				rr_printmsg( VERBOSE_INFO, "%s; trying file from source dir\n",
					printf_msg );
				goto upd_abort;
			}
#endif /* RR_UPDATE_DIR */
#ifdef RR_EMPTY_DELD
		forced_recover:
#endif /* RR_EMPTY_DELD */
#ifdef RR_RECOVER_DELETED
//			if ( rc == ENOENT ) {
//			}
			if ( ( rrchain->err_limit > 0 )	// в принципе можно восстанавливать
				&& ( filelen > 0 ) && ( filetime > 0 )	// ура, есть данные о файле
#ifdef RR_ONLY_IF_NEED_RESTORE
				&& ( outfile != NULL )	// восстанавливать таки надо
#endif /* RR_ONLY_IF_NEED_RESTORE */
				) {
//				infile = file->native_name;
//				GEN_FULL_NAME;
//				file->in_blklen = seclen;
				DUMMY_FILE_PARAMS;
				file->recover_lost = 1;
#ifndef RR_CHECK
				if ( rrchain->lost_blks > rrchain->err_limit ) {
//					fputs( printf_msg, stdout );
					rr_printmsg( VERBOSE_QUIET, "%s; limit of recover data is exceeded\n",
						printf_msg );
					goto err_print;
				} else
#endif /* RR_CHECK */
				{
//					if ( rrchain->verbosity >= VERBOSE_INFO ) {
//						fputs( printf_msg, stdout );
					rr_printmsg( VERBOSE_INFO, "%s; will try to recover file\n",
						printf_msg );
//					}
					goto cont;
				}
#ifdef RR_OPEN_NONPROTECTED
			} else if ( need_flags & NO_RRPROCESS ) {	// даже если нету - ничего страшного
				DUMMY_FILE_PARAMS;
				rr_printmsg( VERBOSE_INFO, "%s; ignored\n",
					printf_msg );
				goto cont;
#endif /* RR_OPEN_NONPROTECTED */
			}
#endif /* RR_RECOVER_DELETED */
//			fputs( printf_msg, stdout );
			rr_printmsg( VERBOSE_QUIET, "%s\n", printf_msg );
#ifndef RR_CHECK
		err_print:
#endif /* RR_CHECK */
//			printf( "\n" );
			goto err2;
		}
	no_open:
		if ( l_stat.st_size > INT_MAX ) {	// длины в rr-list'е и в цепочке - int
			rr_printmsg( VERBOSE_QUIET, "source file %s: too large (%lld bytes)\n",
				fullsrc, (long long) l_stat.st_size );
			goto err3;
		}
		file->len_bytes = ( len = l_stat.st_size );
		if ( len < RIFF_LEN ) {	// слишком короткий для XCD
			exactfiletype = 1;
			goto not_xcd;
		}
#ifdef RR_NO_INIT_READ
		if ( need_flags & NO_INIT_READ )
			goto no_read;
#endif /* RR_NO_INIT_READ */
		if ( _read( /*file->*/in_handle, &riffbuf, pos ) != pos ) {
//			char path_buffer[_MAX_PATH];
			rc = errno;
#ifndef RR_UPDATE_DIR
			rr_printmsg( VERBOSE_INFO,
				"source file %s: initial read failed: %d - %s\n",
				fullsrc, rc, strerror( rc ) );
//			goto err3;
// начинаем эвристический анализ:
#else /* RR_UPDATE_DIR */
			rr_printmsg( VERBOSE_INFO,
				RR_FILETYPE_MSG( " file %s: initial read failed: %d - %s\n" ),
				fullsrc, rc, strerror( rc ) );
			if ( t_base == rrchain->upd_base ) {
				close( in_handle ); in_handle = -1;
				goto upd_abort;
			}
#endif /* RR_UPDATE_DIR */
		no_read:
			exactfiletype = 0;
		}
		if ( exactfiletype ) {
			if ( ( strncmp( riffbuf.riff_tag, "RIFF", 4 ) != 0 ) ||
				( strncmp( riffbuf.type_tag, "CDXAfmt ", 8 ) != 0 ) ||
				( strncmp( riffbuf.data_tag, "data", 4 ) != 0 ) ) {
				goto not_xcd;
			}
#ifdef RR_VCD_AS_ISO
			if ( seclen < XCD_DATA_LEN ) {
				rr_printmsg( VERBOSE_NORMAL, "Warning: "
					RR_FILETYPE_MSG(
						" file %s looks like XCD file instead of ISO file\n"
					), fullsrc );
				goto not_xcd;
			}
#endif /* RR_VCD_AS_ISO */
			in_is_xcd = 1;
		} else {
#ifdef RR_NO_INIT_READ
			if ( !( need_flags & NO_INIT_READ ) )
#endif /* RR_NO_INIT_READ */
				rr_printmsg( VERBOSE_INFO,
					RR_FILETYPE_MSG(" file %s: unable to determine type exactly\n" ),
					fullsrc );
			if ( !in_is_xcd || ( ( ( len - pos ) % XCD_LEN ) != 0 ) ) {
			not_xcd:
//				printf( "source file %s is unlike to be XCD file\n", fnames[ jx ] );
				pos = 0;
				in_is_xcd = 0;
//		goto err3;
			}
		}
#ifndef RR_VCD_AS_ISO
	#ifdef RR_NO_INIT_READ
		#error "Нельзя RR_NO_INIT_READ без RR_VCD_AS_ISO"
	#endif /* RR_NO_INIT_READ */
// блокируем обработку ISO как XCD:
		if ( in_is_xcd && ( seclen < XCD_DATA_LEN ) ) {
			rr_printmsg( VERBOSE_QUIET,
				RR_FILETYPE_MSG( " file %s: cannot process XCD file as ISO\n" ),
				fullsrc );
			goto err3;
		}
#endif /* RR_VCD_AS_ISO */
		if ( in_is_xcd ) {
			need_flags |= XCD_INPUT;
			file->in_blklen = XCD_LEN;
//			file->in_skip_per_blk = XCD_SYNC_LEN + XCD_HDR_LEN + XCD_SUBHDR_LEN;

		} else {
//			file->in_skip_per_blk = 0;
			file->in_blklen = seclen;
		}
		file->len_blks = ( len - pos + file->in_blklen - 1 ) / file->in_blklen;
		file->in_skip_bytes = pos;
// запомним и проверим длину/время
		if ( filelen != -1 ) {
			if ( in_is_xcd ?
				( ( ( filelen + seclen - 1 ) / seclen ) != file->len_blks ) :
				( filelen != l_stat.st_size ) ) {
#ifdef RR_RECOVER_DELETED
				sprintf( printf_msg,
					RR_FILETYPE_MSG( " file %s: given and real sizes don't match" ),
					fullsrc );
				close( /*file->*/in_handle );
						/*file->*/in_handle = -1;
				goto recover;
#else /* RR_RECOVER_DELETED */
#ifdef RR_UPDATE_DIR
	#error "RR_UPDATE_DIR без RR_RECOVER_DELETED не работает!"
#endif /* RR_UPDATE_DIR */
				rr_printmsg( VERBOSE_QUIET,
					"source file %s: given and real sizes don't match\n", fullsrc );
				goto err3;
#endif /* RR_RECOVER_DELETED */
			}
//			file->filesize = filelen;
		} else {
			/*file->filesize*/ filelen = in_is_xcd ? -1 : l_stat.st_size;
		}
//		file->filetime = ( filetime == -1 ) ? stat.st_mtime : filetime;
		if ( filetime == -1 ) filetime = l_stat.st_mtime;

//		file->pos_bytes = -1;
// quick hack:
//		if ( ( errcount > 0 ) && ( jx == ( fcount - 1 ) ) ) {	// последний: ремонтный - м.б. не сначала
//			file->skip_bytes += ( file->len_blks - errcount ) * file->blklen;
//			file->len_blks = errcount;
//		}
		file->read_allow = 1;
	cont:
		file->full_in_name = fullsrc;
		file->in_name = infile;
		file->filesize = filelen; file->filetime = filetime;
	}
	file->rrchain = rrchain;

//	pad_blks = need_align ? ( RR_STREAMNUM - ( 1 + ( file->len_blks - 1 )
//		% RR_STREAMNUM ) ) : 0;

	pad_blks = ( need_flags & NEED_ALIGN ) ? ( ( RR_STREAMNUM - 1 ) -
			( ( rrchain->total_blks + file->len_blks - 1 ) % RR_STREAMNUM ) ) : 0;

	if ( rr_chksize( rrchain, file->len_blks + pad_blks ) != NO_ERROR )
		goto err3;

//	file->need_flags = need_flags;
	file->blk_from = rrchain->total_blks + pad_blks;
//			( 1 + ( rrchain->total_blks - 1 ) / RR_STREAMNUM ) * RR_STREAMNUM;
	if ( ( need_flags & NO_COPYPLAIN ) && !in_is_xcd ) {
		outfile = NULL;
		if ( !( sector.type & MISSING ) )
			sector.type = DUMMY;
	}
	if ( outfile != NULL ) {
		char *fulltgt = native_tgt;
		int out_is_xcd, out_need_xcd;

// 		_makepath( fulltgt, NULL, rrchain->tgt_base, outfile, NULL );
		if ( rr_fullname( fulltgt, rrchain->tgt_base, outfile, NULL, "target file",
				1 ) == NULL )
			goto err3;

		out_need_xcd = ( need_flags & NO_XCDDECODE ) ? in_is_xcd : 0;
		out_is_xcd =
			!!rr_isxcd( fulltgt, out_need_xcd ? xcd_tgt : NULL,
				INFILE_XCD2 rrchain->xcd_base );
		if ( out_need_xcd && !out_is_xcd ) {
			fulltgt = xcd_tgt; out_is_xcd = 1;
		}

		if ( out_is_xcd ) {
			int filelen = XCD_START + file->len_blks * XCD_LEN;
// блокируем обработку ISO как XCD:
			if ( seclen < XCD_DATA_LEN ) {
				rr_printmsg( VERBOSE_QUIET,
					"target file %s: cannot process ISO file as XCD\n", fulltgt );
				goto err3;
			}
			need_flags |= XCD_OUTPUT;
//			if ( /* !exactfiletype || */ ( !in_is_xcd
//				/* file->blklen != XCD_LEN */ ) ) {	// кодируем в XCD
/*					printf( "source file %s, target file %s: "
					"encoding to XCD is not supported yet!\n",
					file->name, file->out_name );
				goto err3;*/
			memcpy( &riffbuf, &riffhdr, XCD_START );
			riffbuf.rifflen = filelen - 8;
			riffbuf.datalen = filelen - 0x2C;
//			}
			file->out_blklen = XCD_LEN;
			file->out_skip_bytes = XCD_START;
//			file->out_skip_per_blk = XCD_SYNC_LEN + XCD_HDR_LEN + XCD_SUBHDR_LEN;
		} else {
			file->out_blklen = seclen;
			file->out_skip_bytes = 0;
//			file->out_skip_per_blk = 0;
		}
// создать каталог:
		if ( rr_makedestdir( fulltgt ) != 0 ) {
			rr_printmsg( VERBOSE_QUIET,
				"target file %s: error creating dir\n", fulltgt );
			goto err3;
		}
		if ( ( out_handle = rrf_open( fulltgt, _O_CREAT | _O_RDWR, need_flags,
						"target", printf_msg, NULL, &fmode ) ) == -1 ) {
			rr_printmsg( VERBOSE_QUIET, "%s\n", printf_msg );
			goto err3;
		}
		file->write_allow = 1;
		file->full_out_name = fulltgt;
		file->out_name = outfile;
		if ( file->out_blklen == XCD_LEN ) {
//				char *skip_buf = malloc( file->skip_bytes );
			if ( ( rc = rrh_write( /*file->*/out_handle, (char *) &riffbuf, XCD_START ) ) < 0 ) {
				rc = -rc;
				rr_printmsg( VERBOSE_QUIET,
					"target file %s: initial write failed: %d - %s\n",
					fulltgt, rc, strerror( rc ) );
				goto err4;
			}
		}
// %%% здесь можно сделать копирование skip'нутой части ремонтного файла
	} else {
		/*file->*/out_handle = -1;
		file->out_name = NULL;
		file->out_blklen = file->out_skip_bytes = /*file->out_skip_per_blk =*/ 0;
	}
	file->out_blks = 0;
	file->need_flags = need_flags;
//	sector.rfile = file->itemnum;
//	sector.type = UNTOUCHED;

#ifdef RR_TRACK_PATH
	#error "выделять компоненту пути, хранить только имя и указатель на путь "
	#error "(путь хранить, только если обновлён), следить за вложенностью и повторами"
#endif /* RR_TRACK_PATH */
	rr_filesize = sizeof( rritem_t )
#ifdef RR_STORE_EDC
		+ ( sizeof( file->edc_array[ 0 ] ) * local_file.len_blks )
#endif /* RR_STORE_EDC */
#ifndef RR_PERSISTENT_NAMES
		+ ( ( file->in_name == NULL ) ? 0 : strlen( file->in_name ) + 1 )
		+ ( ( file->out_name == NULL ) ? 0 : strlen( file->out_name ) + 1 )
		+ ( ( file->native_name == NULL ) ? 0 : strlen( file->native_name ) + 1 )
		+ ( ( file->full_in_name == NULL ) ? 0 : strlen( file->full_in_name ) + 1 )
		+ ( ( file->full_out_name == NULL ) ? 0 : strlen( file->full_out_name ) + 1 )
#endif /* RR_PERSISTENT_NAMES */
		;
// выделяем память, копируем...
	if ( ( file = malloc( rr_filesize ) ) == NULL ) {
		rr_printmsg( VERBOSE_QUIET,
			"RR-chain %s: not enough memory to get %d bytes for next file\n",
			rrchain->rrname, rr_filesize );
		goto err5;
	}
	memcpy( file, &local_file, sizeof( rritem_t ) );

#ifdef RR_STORE_EDC
	memset( file->edc_array, 0,
		sizeof( file->edc_array[ 0 ] ) * local_file.len_blks );
	#ifndef RR_PERSISTENT_NAMES
	dyn_tname = (char *) &file->edc_array[ local_file.len_blks ];
	#endif /* RR_PERSISTENT_NAMES */
#else /* RR_STORE_EDC */
	#ifndef RR_PERSISTENT_NAMES
	dyn_tname = file->names;
	#endif /* RR_PERSISTENT_NAMES */
#endif /* RR_STORE_EDC */

#ifndef RR_PERSISTENT_NAMES
	#define COPY_NAME( name ) \
		do { \
			if ( local_file.name != NULL ) { \
				file->name = dyn_tname; \
				strcpy( (char *) file->name, local_file.name ); \
				dyn_tname += strlen( local_file.name ) + 1; \
			} \
		} while (0)
#else /* RR_PERSISTENT_NAMES */
#error "Посмотреть, как хранятся имена"
	#define COPY_NAME( name ) \
		do { \
			file->name = local_file.name; \
		} while (0)
#endif /* RR_PERSISTENT_NAMES */

	COPY_NAME( in_name );
	COPY_NAME( out_name );
	COPY_NAME( native_name );
	COPY_NAME( full_in_name );
	COPY_NAME( full_out_name );

	sector.rritem = file;
	list_add_tail( &file->list_item, &rrchain->items_list );
	if ( rrchain->max_blklen < file->native_blklen )
		rrchain->max_blklen = file->native_blklen;
	rrchain->total_files++;
	rrchain->total_bytes += file->len_bytes;
	rrchain->total_blks += pad_blks + file->len_blks;

	for ( jy = file->blk_from; jy < rrchain->total_blks; jy++ ) {
		rrchain->blk_map[ jy ] = sector;
	}
//	file->out_len_bytes = ( file->out_blklen == XCD_LEN ) ? -1 : outlen;
// Утрясём длину и дату/время файла:
//	file->filetime = filetime;
//	file->filelen = filelen;
	if ( out_handle != -1 )
		close( out_handle );
	if ( in_handle != -1 )
		close( in_handle );
	return ( file );

err5:
	goto err4a;
err4:
	close( /*file->*/out_handle );
err4a:
	unlink( file->full_out_name );
//	unlink( file->out_name );
err3:
	close( /*file->*/in_handle );
err2:
//	free( file );
err1:
	return ( NULL );
}

int rr_blklen( rrchain_t *rrchain ) {
	return ( rrchain->max_blklen );
}

static void rr_errmsg( int *rc, rritem_t *file,
int errcode, char *comment ) {
	if ( errcode < 0 ) {
		int err = errno;
		rr_printmsg( VERBOSE_NORMAL, "target file %s: %s failed: %d - %s\n",
			file->full_out_name, comment, err, strerror( err ) );
//					goto err3;
		if ( rc != NULL )
			*rc = SOFT_ERROR;
	}
}
static void rr_shape_outfile( rritem_t *file, int *rc, int settime ) {
//	int rc = NO_ERROR;
	int need_setsize =
		( file->out_blks == file->len_blks ) &&
		( file->filesize != -1 ) && ( file->out_blklen != XCD_LEN );
	if ( ( ( need_setsize )	// размер
			|| ( settime && ( file->filetime != (time_t) -1 ) ) ) ) {	// время
		int out_handle;
		char outbuf[ RRF_MSG_LEN ];
		if ( ( ( out_handle = rrf_open( file->full_out_name, _O_RDWR,
							file->need_flags, "target", outbuf, NULL, NULL ) ) == -1 ) ) {	// не открылся
//					has_error = 1; discard = 1;
			rr_printmsg( VERBOSE_NORMAL, "%s\n"
				" - resize/settime cannot be performed\n", outbuf );	// сообщить
//					goto error;
		} else {
			if ( need_setsize ) {
				rr_errmsg( rc, file, _chsize( /*file->*/out_handle, file->filesize ),
					"resize" );
			}
			if ( settime && ( file->filetime != (time_t) -1 ) &&
				( file->out_blks == file->len_blks ) ) {
				int iret;
#ifdef __WIN32__
				struct _utimbuf ftime;
				time( &ftime.actime );
				ftime.modtime = file->filetime;
				iret = _futime ( out_handle, &ftime );
#else
				//ftime[0] specifies the new access time,
				//ftime[1] specifies the new modification time.
				struct timeval ftime[2];
				time( &ftime[0].tv_sec);// ??? зачем такие сложности? //так в Glibc
				ftime[0].tv_usec = 0L;
				ftime[1].tv_sec =  file->filetime;
				ftime[1].tv_usec = 0L;
				iret = futimes ( out_handle, ftime);
				if (iret == -1){
					iret = errno;
					iret = -1;
				}
#endif	/*__WIN32__*/
				rr_errmsg( rc, file,iret,"settime" );
			}
			close( /*file->*/out_handle );
		}
	}
}
static int rr_bad_count( rrchain_t *rrchain, int blk_from, int blk_to ) {
	// считаем нескопированные блоки в диапазоне от-до
	int count;
	for ( count = 0; blk_from < blk_to; blk_from++ ) {
		if ( !( rrchain->blk_map[ blk_from ].type & COPIED ) )
			count++;
	}
	return ( count );
}
static int rr_finish( rrchain_t *rrchain, int discard ) {
	struct list_head *jp;
	rritem_t *jf = NULL;
	int rc = NO_ERROR;

	rr_select( rrchain, NULL, NULL );
//error:
list_for_each( jp, &rrchain->items_list ) {
	rritem_t *file = list_entry( jp, rritem_t, list_item );
	if ( file->write_allow /*full_out_name[ 0 ] != '\0'*/ ) {	// есть выходной файл
		if ( discard ) {
			unlink( file->full_out_name );
		} else {	// закрытие; переименовывает недовосстановленные файлы
			rr_shape_outfile( file, &rc, 1 );
			if ( rr_bad_count( rrchain, file->blk_from,
					file->len_blks + file->blk_from - 1 ) ) {	// файл битый
				char bad_name[ _MAX_PATH + 4 ];							// переименовываем
				strcpy( bad_name, file->full_out_name );
				strcat( bad_name, ".bad" );
				rename( file->full_out_name, bad_name );
			}
		}
	}

	list_del( jp );
	if ( jf != NULL )
		free( jf );
	jf = file;
}
if ( jf != NULL )
	free( jf );
free( rrchain );
return ( /*NO_ERROR*/ rc );
}
int rr_discard( rrchain_t *rrchain ) {
	return ( rr_finish( rrchain, 1 ) );
}

void rr_print( rrchain_t *rrchain ) {
	struct list_head *jp;
	int files_lost = 0, bytes_lost = 0, blks_lost = 0;
//	int verbose_level = VERBOSE_NORMAL;
//	if ( rrchain->verbosity < VERBOSE_INFO )
//		return;
//goto totals;
	rr_printmsg( VERBOSE_INFO, "Processing files:\n" );
list_for_each( jp, &rrchain->items_list ) {
//		char
//			*tmp;
	rritem_t *file = list_entry( jp, rritem_t, list_item );
	if ( file->recover_lost ) {
			/* verbose_level = VERBOSE_QUIET; */ files_lost++;
		bytes_lost += file->len_bytes; blks_lost += file->len_blks;
	}
	rr_printmsg( VERBOSE_INFO, "%s (%s, %d sectors",
		( file->full_in_name == NULL )
		? "(unnamed)" : file->full_in_name,
		( file->in_blklen < XCD_DATA_LEN ) ? "ISO" : "XCD",
		file->len_blks );
	if ( file->len_blks > 0 )
		rr_printmsg( VERBOSE_INFO, " (%d...%d)"
					/* ", %d bytes skipped)" */,
			file->blk_from, file->blk_from + file->len_blks - 1
			/* ,file->in_skip_bytes */ );
	rr_printmsg( VERBOSE_INFO, ")%s",
		file->recover_lost ? " -- FILE LOST" : "" );
	if ( file->out_name != NULL ) {
		rr_printmsg( VERBOSE_INFO, "\n  -> %s (expect %d bytes)",
			file->full_out_name, ( file->len_blks
#ifndef RR_NEW_ENC
				+ ( ( rrchain->rrheader == file ) ? rrchain->rrcount : 0 )
#endif /* RR_NEW_ENC */
			) * file->out_blklen );
	}
	rr_printmsg( VERBOSE_INFO, "\n" );
}
//totals:
rr_printmsg( ( files_lost > 0 ) ? VERBOSE_QUIET : VERBOSE_NORMAL,
	"Total: %d input files, %d sectors, %ld bytes\n",
	rrchain->total_files, rrchain->total_blks, rrchain->total_bytes );
if ( files_lost > 0 )
	rr_printmsg( VERBOSE_QUIET, "Lost: %d files, %d sectors, %ld bytes\n",
		files_lost, blks_lost, bytes_lost );
}

static __inline unsigned int rr_edc( const rrblock_t *block ) {
	return build_edc( block->subheader, 0, XCD_SUBHDR_LEN + XCD_DATA_LEN );
}

#define MAKE_DUMMY_BLOCK( ptr ) \
	do { \
		rrblock_t *blk = (ptr); \
		memset( blk, 0, XCD_LEN ); \
		memcpy( blk->sync, XCD_SYNC_FIELD, XCD_SYNC_LEN ); \
		blk->edc = 0x55555555; \
	} while (0)

static int rr_enblock(	// распихивает содержимое входного буфера по XCD-блокам
rrblock_t *out, const char *in, int blklen, int count ) {
	int jx;
	if ( in != NULL ) for ( jx = 0; jx < count; jx++, out++, in += blklen ) {
			int taillen = XCD_DATA_LEN - blklen;	// хвост блока (заполняем нулями)
			memcpy( out->sync, XCD_SYNC_FIELD, XCD_SYNC_LEN );
			memset( out->header, 0, XCD_HDR_LEN + XCD_SUBHDR_LEN );
			memcpy( out->data, in, blklen );
			if ( taillen > 0 )
				memset( out->data + blklen, 0, taillen );
			out->edc = rr_edc( out );
		} else for ( jx = 0; jx < count; jx++, out++ ) {	// фиктивный блок
		MAKE_DUMMY_BLOCK( out );
	}
	return ( count );
}
/*__inline*/ static int rr_goodscan(	// return: count of good blocks or < 0 if none good
	const char *buf,
	int datalen,				// длина полезной части
	int blklen,					// полная длина блока (шаг)
int count ) {
	int jx, jy;
	for ( jx = 0; jx < count; jx++, buf += blklen ) {
		int nonzero = 0;
		for ( jy = 0; jy < datalen; jy++ ) {
			if ( buf[ jy ] != 0 ) {
				nonzero = 1;
				break;
			}
		}
		if ( nonzero )
			continue;
		return ( ( jx == 0 ) ? HARD_ERROR : jx );
	}
	return ( jx );
}
static int rr_deblock(	// return: count of good blocks or < 0 if none good
	char *out,
	rrblock_t *in,	// битые блоки затираются (MAKE_DUMMY_BLOCK)
	int blklen,
	int count
		/*, int *errcount*/ ) {
	int jx, rc = 0;
	for ( jx = 0; jx < count; jx++, in++, out += blklen ) {
//		memcpy( out, in->data, blklen );
		if ( ( memcmp( in->sync, XCD_SYNC_FIELD, XCD_SYNC_LEN ) != 0 ) ||
			( in->edc != rr_edc( in ) ) ) {
			MAKE_DUMMY_BLOCK( in );	// НЕ СОХРАНЯТЬ!!! битый пакет в копию
			memcpy( out, in->data, blklen );
			rc = SOFT_ERROR;
			break;
//			if ( errcount ) (*errcount)++;
		}
		memcpy( out, in->data, blklen );
	}
	if ( ( rc < 0 ) && ( jx == 0 ) )	// первый же сектор - дохлый
		jx = rc;
	return ( jx );
}
static int rr_makecopy(	// записывает содержимое буфера в файл-копию
	rrchain_t *rrchain,
	rrmap_t rfnum,
	int fileblk,	// только для справки (вывод в лог)
	int blks,
	const char *buf,		// или NULL, если надо сделать специально сбойный блок
	const rrmap_t *map,	// или NULL, если надо записать исправления
int *error ) {
	rritem_t *file = rfnum.rritem;
	int rc = NO_ERROR;
	char *tbuf = NULL;	// %%% для поиска глюки
	int tblklen, tblks = blks;
	int out_handle = rrchain->active_out_handle;
//		blklen = file->native_blklen;
	if ( /*file->*/out_handle == -1 )	// файла-копии нет - можно не писать
		return ( blks );
	if ( file->out_blklen == XCD_LEN  ) {
		tblklen = XCD_LEN; tblks = XCD_BUFBLKS; tbuf = rrchain->xcd_buf;
//		if ( tblks > blks )
//			tblks = blks;
		if ( buf == NULL ) {	// генерируем пустышку
			rr_enblock( (rrblock_t *) tbuf, NULL, rrchain->max_blklen, tblks );
		}
	} else {	// для не-XCD-файлов
//		tlen = blklen;
		if ( buf == NULL ) {	// генерируем пустышку
			tbuf = rrchain->xcd_buf;
			tblks = ( XCD_BUFBLKS * XCD_LEN ) / file->native_blklen;
//			if ( tblks > blks )
//				tblks = blks;
			tblklen = file->native_blklen;
			memset( tbuf, 0, tblks * tblklen );
		} else if ( file->native_blklen == rrchain->max_blklen ) {	// ура, можно писать всё сразу
			tblklen = file->native_blklen;
		} else {	// через xcd_buf
			tbuf = rrchain->xcd_buf;
			tblks = ( XCD_BUFBLKS * XCD_LEN ) / file->native_blklen;
			tblklen = file->native_blklen;
//			tblks = 1;
		}
	}

	while ( blks > 0 ) {	// собственно пишем файл
		int wr_len;
		if ( tblks > blks )
			tblks = blks;
		if ( buf == NULL ) {	// tbuf - пустышка, tblklen - длина ейного блока, tblks - кол-во
		} else {
			if ( file->out_blklen == XCD_LEN ) {
//#error "Копировать из buf при rr_write"
				if ( file->in_blklen != XCD_LEN ) {	// надо собирать в блок
					rr_enblock( (rrblock_t *) rrchain->xcd_buf, buf,
						rrchain->max_blklen, tblks );
					if ( map != NULL ) {
						int jx;
						for ( jx = 0; jx < tblks; jx++ )
							if ( map[ jx ].type /*< DUMMY*/ & NOTCOPIED )
//								( (rrblock_t *) tbuf )[ jx ].edc ^= 0x55555555;
								MAKE_DUMMY_BLOCK( ( (rrblock_t *) tbuf ) + jx );
	//					map = NULL;
						map += tblks;
					}
				} else if ( map == NULL ) {	// XCD->XCD, запись исправлений
					rr_enblock( (rrblock_t *) rrchain->xcd_buf, buf,
						rrchain->max_blklen, tblks );
				}
			} else if ( file->native_blklen != rrchain->max_blklen ) {	// через xcd_buf
				const char *buf2 = buf;
				char *tbuf2 = tbuf;
				int jx;
				for ( jx = 0; jx < tblks; jx++, tbuf2 += file->native_blklen,
					buf2 += rrchain->max_blklen ) {
					memcpy( tbuf2, buf2, file->native_blklen );
				}
			} else {
				tbuf = (char *) buf;
			}
		}

		if ( ( wr_len = rrh_write(
				/*file->*/out_handle, tbuf, tblklen * tblks ) ) < 0 ) {
			wr_len = -wr_len;
			if ( error != NULL ) *error = wr_len;
			rr_printmsg( VERBOSE_QUIET | CLEAR_LINE,
				"target file %s, sector %d: error writing: %d - %s\n",
				file->out_name, fileblk, wr_len, strerror( wr_len ) );
			rc = FATAL_ERROR;
			break;
		}
		file->has_data = 1;
		fileblk += tblks; blks -= tblks;
		if ( buf != NULL ) buf += rrchain->max_blklen * tblks;
	}
	if ( file->out_blks < fileblk )
		file->out_blks = fileblk;

	return ( rc );
}
static int rr_seekcopy( rrchain_t *rrchain, rrmap_t rfnum, int fileblk,
int *error ) {
	rritem_t *file = rfnum.rritem;
	int tblk = fileblk;
	int out_handle = rrchain->active_out_handle;
	if ( /*file->*/out_handle != -1 ) {
		if ( file->out_blks < tblk )
			tblk = file->out_blks;
		if ( _lseek( /*file->*/out_handle,
				tblk * file->out_blklen + file->out_skip_bytes, SEEK_SET ) == -1L ) {
			if ( error != NULL ) *error = errno;
			rr_printmsg( VERBOSE_QUIET | CLEAR_LINE,
				"%s file %s, sector %d: error seeking: %d - %s\n", "target",	// для экономичности
				file->out_name, fileblk, *error, strerror( *error ) );
			return ( FATAL_ERROR );
		}
		if ( tblk < fileblk ) {	// заполним нулями
			int jx = rr_makecopy( rrchain, rfnum, tblk, fileblk - tblk, NULL,
					rrchain->blk_map + file->blk_from + tblk, error );
			if ( jx < NO_ERROR )
				return ( jx );
//			file->out_blks = fileblk;
		}
	}
	return ( NO_ERROR );
}
/*__inline*/ static int rr_readerr(
#ifdef RR_FILL_BAD
	rrchain_t *rrchain,
#endif /* RR_FILL_BAD */
	rrmap_t rfnum, int blks_read, int *error, rrmap_t *loop_map,
		/*int *errcount,*/ int rc, int fileblk, const char *filename,
int badcount ) {
	int jx;
//	int verbosity = rfnum.rritem->rrchain->verbosity;
	*error = errno;
	if ( badcount <= 1 ) {
		badcount = 1;
		rr_printmsg(
			( ( rfnum.type /*> DUMMY*/ & COPIED ) ? VERBOSE_QUIET : VERBOSE_NORMAL )
			| CLEAR_LINE,
			"%s file %s, sector %d: error reading: %d - %s\n",
			( rfnum.type /*> DUMMY*/ & COPIED ) ? "target" : "source", filename,
			fileblk + blks_read, *error, strerror( *error ) );
		loop_map[ blks_read ].type = BAD;
	} else {
		rr_printmsg(
			( ( rfnum.type /*> DUMMY*/ & COPIED ) ? VERBOSE_QUIET : VERBOSE_NORMAL )
			| CLEAR_LINE,
			"%s file %s, sectors %d...%d: possibly are bad\n",
			( rfnum.type /*> DUMMY*/ & COPIED ) ? "target" : "source", filename,
			fileblk + blks_read, fileblk + blks_read + badcount - 1 );
		for ( jx = 0; jx < badcount; jx++ )
			loop_map[ blks_read + jx ].type = SHADY;
	}
//	if ( errcount ) (*errcount)++;	// += badcount;
//				if ( rfnum.type > DUMMY )
//readerr:
	if ( rfnum.type /*> DUMMY*/ & COPIED ) {
//					rrchain->blk_map[ rrchain->pos_blks ] = -rfnum;
		for ( jx = 0; jx < badcount; jx++ )
			loop_map[ blks_read + jx ].type = FATAL;
		rc = FATAL_ERROR;
	}
#ifdef RR_FILL_BAD
	else {
		int jt = rr_makecopy( rrchain, rfnum, fileblk, badcount, NULL,
				loop_map, error );
		if ( jt < NO_ERROR )
			rc = jt;
	}
#endif /* RR_FILL_BAD */
	return ( rc );
}

#ifdef RR_NEW_ENC
static int rr_lockchain( rrchain_t *rrchain ) {
}
#endif /* RR_NEW_ENC */
int rr_read( rrchain_t *rrchain, char *buf, int blks, int *errcount
#ifdef RR_SHOW_LOST
	, int *lostcount
#endif /* RR_SHOW_LOST */
) {
	rrmap_t rfnum;				// real filenum
	int
	fileblk;			// номер начального блока в файле
	int len, len2;
	int total_blks_read/*, file_blks_read = 0*/;
	int error = 0;
	int rc = HARD_ERROR;
//	int max_blklen = rrchain->max_blklen;
	int dmask =
#ifdef RR_NEW_ENC
		GENERATED |
#endif /* RR_NEW_ENC */
		( ( rrchain->read_blks > 1 ) ?
			(	DECOY_BLK | FATAL | NOTREADABLE | MISSING | BAD | SHADY | DUMMY ) :
			(	DECOY_BLK | FATAL | NOTREADABLE | MISSING | BAD | DUMMY ) );
//	char printf_msg[ RRF_MSG_LEN ];
#ifdef RR_NEW_ENC
	if ( rrchain->blk_map == NULL )

#endif /* RR_NEW_ENC */
		rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	if ( errcount != NULL ) *errcount = 0;
#ifdef RR_SHOW_LOST
	if ( lostcount != NULL ) (*lostcount) = 0;
#endif /* RR_SHOW_LOST */
//	_commit( file->in_handle );
	if ( rrchain->pos_blks < 0 ) {
		error = EINVAL; rc = FATAL_ERROR;
		goto err2;
	}
	if ( ( blks == 0 )	// ничего не заказано
		|| ( rrchain->pos_blks >= rrchain->total_blks ) )	// уже конец объекта
		return 0;
	if ( rrchain->pos_blks + blks > rrchain->total_blks )	// слишком много заказано
		blks = rrchain->total_blks - rrchain->pos_blks;
	for ( total_blks_read = 0; ( total_blks_read < blks ) && ( error == 0 );
			/*total_blks_read += file_blks_read*/ ) {
		int /*loops = 1,*/ pos;
		int
		jx,
		fileblkcount,	// кол-во блоков для читать из конкретно этого файла
		loopblkcount,	// кол-во блоков за один заход
//			filenum,			// номер файла в объекте
		handle,
		loop_blks_read;	// прочитано блоков за один вызов read
		rritem_t *file = NULL;
//		rrblock_t
//			tmpbuf,
		char *readptr = buf;
		const char *filename;
		int blklen, len_blks;
		int endloop_flag = 0, dummy_flag = 0;
		int decoy_flag = 0;
		char *decoy_ptr;
/*
		for ( filenum = rrchain->total_files - 1; filenum >= 0; filenum-- ) {
			if ( rrchain->files[ filenum ].blk_from <=
					( rrchain->pos_blks + total_blks_read ) )
				break;	// нужный номер - в filenum
		}
*/
	retry:
		rfnum = rrchain->blk_map[ rrchain->pos_blks + total_blks_read ];
// another quick hack:
		if (  rfnum.type & dmask ) {			// dummy / bad / rough
//dummy_file:
			fileblkcount = MAX_BLKNUM;	// больше точно не понадобится
			dummy_flag = 1;
		} else {	// реальный файл
//			filenum = rfnum.rfile;
			file = rfnum.rritem;
			fileblk = ( rrchain->pos_blks + total_blks_read ) - file->blk_from;
			if ( ( jx = rr_select( rrchain, file, &error ) ) < SOFT_ERROR ) {
				rc = jx;
				goto err1;
			} else if ( jx < NO_ERROR )	// т.е. SOFT_ERROR => src-файл не удалось открыть
				goto retry;
			if ( file->need_flags & DECOY_ITEM ) {
				decoy_flag = 1;
				fileblkcount = file->len_blks - fileblk;
				decoy_ptr = file->decoy_buf + file->native_blklen * fileblk;
			} else {
				if ( ( rfnum.type & NOTCOPIED ) ||
					( rrchain->active_out_handle == -1 ) ) {	// оригинальный
					handle = rrchain->active_in_handle;
					filename = file->full_in_name;
					blklen = file->in_blklen;
					len_blks = file->len_blks;
					pos = fileblk * blklen + file->in_skip_bytes;
				} else {	// уже прочитано - берём копию
					handle = rrchain->active_out_handle;
					filename = file->full_out_name;
					blklen = file->out_blklen;
					len_blks = file->out_blks;
	//				pos = fileblk * OGM_LEN;
					pos = fileblk * blklen + file->out_skip_bytes;
				}
				if ( _lseek( handle, pos, SEEK_SET ) == -1L ) {
					char *type = "source";
					int mask = VERBOSE_NORMAL | CLEAR_LINE;
					rrchain->blk_map[ file->blk_from + fileblk ].type = BAD;
					if ( rfnum.type /*> DUMMY*/ & COPIED ) {
						type = "target";
						mask = VERBOSE_QUIET | CLEAR_LINE;
						rrchain->blk_map[ file->blk_from + fileblk ].type = FATAL;
						rc = FATAL_ERROR;
					}
					if ( errcount ) (*errcount)++;
					error = errno;
					rr_printmsg( mask, "%s file %s, sector %d: error seeking: %d - %s\n",
						type, filename, fileblk, error, strerror( error ) );
					if ( rfnum.type /*> DUMMY*/ & COPIED ) {
						rc = FATAL_ERROR;
						goto err1;
					}
					if ( total_blks_read == 0 )
						goto err1;
					goto ok;
				}
				if ( rfnum.type /*< DUMMY*/ & NOTCOPIED ) {
					int jt = rr_seekcopy( rrchain, rfnum, fileblk, &error );
					if ( jt < NO_ERROR ) {
						if ( errcount ) (*errcount)++;
						rrchain->blk_map[ file->blk_from + fileblk ].type = FATAL;
	//					if ( total_blks_read > 0 )
	//						goto ok;
						rc = jt; goto err1;
					}
				}
				fileblkcount = len_blks - fileblk;
			}
		}
		if ( fileblkcount > ( blks - total_blks_read ) )
			fileblkcount = blks - total_blks_read;
		jx = rr_typelen( rrchain, rrchain->pos_blks + total_blks_read, fileblkcount );
		if ( fileblkcount > jx )
			fileblkcount = jx;
/*
		for ( jx = 0; jx < fileblkcount; jx++ ) {	// ищем конец блока
			if ( !RRMAP_EQ( rfnum,
					rrchain->blk_map[ rrchain->pos_blks + total_blks_read + jx ] ) ) {
				fileblkcount = jx;
				break;
			}
		}
*/
// quick hack again:
		if ( /*rfnum.file == -1*/ dummy_flag ) {
			len = fileblkcount * rrchain->max_blklen;
			memset( buf, 0, len );
			buf += len; total_blks_read += fileblkcount;
#ifdef RR_SHOW_LOST
			if ( ( rfnum.type & MISSING ) && ( lostcount != NULL ) )
				(*lostcount) += fileblkcount;
			else
#endif /* RR_SHOW_LOST */
			if ( ( rfnum.type & ( NOTCOPIED & ~DECOY_BLK ) ) && ( errcount != NULL ) )
				(*errcount) += fileblkcount;
			continue;
		}
		if ( decoy_flag ) {
			len = fileblkcount * file->native_blklen;
			memcpy( buf, decoy_ptr, len );
			buf += len; total_blks_read += fileblkcount;
			continue;
		}
		loopblkcount = /*XCD_BUFBLKS*/ rrchain->read_blks;
		if ( ( blklen == XCD_LEN )	// обрабатываемый файл - типа XCD
			|| ( file->native_blklen != rrchain->max_blklen ) ) {
			readptr = rrchain->xcd_buf;
#ifdef RR_OLD_BLOCK
			loopblkcount = /*XCD_BUFBLKS*/ rrchain->read_blks;
		} else if ( rrchain->read_blks == 1 ) {
			loopblkcount = rrchain->read_blks;
		} else {
//			loopblkcount = fileblkcount;	// %%% quick hack
			loopblkcount = /*XCD_BUFBLKS*/ rrchain->read_blks;
#endif /* RR_OLD_BLOCK */
		}
		while ( fileblkcount > 0 ) {
			rrmap_t *loop_map = rrchain->blk_map + fileblk + file->blk_from;
			loop_blks_read = 0;
#if 0
#ifdef RR_FIN_WKARND
			if ( loopblkcount >= ( rrchain->total_blks - ( file->blk_from + fileblk ) ) )
				loopblkcount = 1;
			else
#endif /* RR_FIN_WKARND */
#endif /*0*/
			if ( loopblkcount >= fileblkcount ) {
#ifdef RR_FIN_WKARND
				if ( ( blklen == XCD_LEN ) &&
					( loopblkcount >= ( file->len_blks - fileblk ) ) )
					loopblkcount = 1;
				else
#endif /* RR_FIN_WKARND */
					loopblkcount = fileblkcount;
			}
#ifdef RR_DECODETEST
			if ( fileblk == 3586 ) {
				loopblkcount = 1;
			}
#endif /* RR_DECODETEST */
			errno = 0; len = loopblkcount * blklen;
			if ( readptr != rrchain->xcd_buf )
				readptr = buf;
			len2 = _read( handle, readptr, len );
			if ( len2 < 0 ) {
#ifdef __WIN32__
			readerr:	// goto из ветки _eof() ниже
#endif /* __WIN32__ */
				rc = rr_readerr(
#ifdef RR_FILL_BAD
						rrchain,
#endif /* RR_FILL_BAD */
						rfnum, loop_blks_read, &error, loop_map, /*errcount,*/
						rc, fileblk, filename, loopblkcount );
				if ( ( rc > FATAL_ERROR ) && ( total_blks_read > 0 ) )
					goto ok;
				goto err1;
/*seek_err:
				memset( buf, 0, OGM_LEN );
				total_blks_read++; buf += OGM_LEN;
				goto main_err;*/
			}
			loop_blks_read = len2 / blklen; //len2 %= blklen;
			if ( loop_blks_read < loopblkcount ) {	// прочитали меньше, чем надо
#ifdef __WIN32__
				if ( !_eof( handle ) ) {
					if ( loop_blks_read == 0 )
						goto readerr;
					rc = rr_readerr(
#ifdef RR_FILL_BAD
							rrchain,
#endif /* RR_FILL_BAD */
							rfnum, loop_blks_read, &error, loop_map, /*errcount,*/ rc,
							fileblk, filename, 1 );
//					if ( errcount ) (*errcount)++;
//					if ( ( total_blks_read + loop_blks_read ) == 0 )	// ничего не прочитали
//						goto readerr;
					if ( rc <= FATAL_ERROR )
						goto err1;
					endloop_flag = 1;
				} else
#endif /*__WIN32__*/
				{	// EOF
					if ( ( ( len2 + blklen ) <= len )	// не хватает целого блока или более
#ifndef RR_CUT_COPY
						|| ( rfnum.type /*> DUMMY*/ & COPIED )	// EOF в копии
#endif /* RR_CUT_COPY */
						|| ( blklen == XCD_LEN ) ) {	// EOF в XCD
						// значит, что-то не то:
						error = EILSEQ;
						loop_map[ loop_blks_read ].type = BAD;
						if ( errcount )
							(*errcount)++;
						rr_printmsg(
							( ( rfnum.type /*> DUMMY*/ & COPIED ) ?
								VERBOSE_QUIET : VERBOSE_NORMAL ) | CLEAR_LINE,
							"%s file %s, sector %d: unexpected EOF\n",
							( rfnum.type /*> DUMMY*/ & COPIED ) ? "target" : "source",
							filename, fileblk );
	//					rc = FATAL_ERROR;	// %%% лучше пусть восстанавливает
						if ( rfnum.type /*> DUMMY*/ & COPIED ) {
//							rrchain->blk_map[ rrchain->pos_blks ] = -rfnum;
							rc = FATAL_ERROR;
//err1a:
							loop_map[ loop_blks_read ].type = FATAL;
//							fputs( printf_msg, stdout );
							goto err1;
						}
//						if ( rrchain->verbosity >= VERBOSE_INFO )
//							fputs( printf_msg, stdout );
// %%% нельзя просто так уходить на ok ! use endloop_flag instead
						if ( total_blks_read == 0 )
							goto err1;
						endloop_flag = 1;
					} else {
// легальный EOF в не-XCD-файле:
						if ( ( len2 % blklen ) > 0 ) {
							memset( readptr + len2, 0, blklen - ( len2 % blklen ) );
							loop_blks_read++; //buf += OGM_LEN;
						}
					}
				}
			}

			if ( blklen == XCD_LEN ) {
				int local_blks = loop_blks_read, local_ready;
				int start_blk = 0;
				char *tbuf = buf;
				rrblock_t *tptr = (rrblock_t *) readptr;
				while ( local_blks > 0 ) {
					local_ready = rr_deblock( tbuf, tptr, file->native_blklen,
							local_blks );
					if ( local_ready < local_blks ) {	// кривой сектор
						if ( local_ready < 0 )
							local_ready = 0;
//						rr_markrough( fileblk + file->blk_from + local_ready );
						if ( errcount ) (*errcount)++;
						rr_printmsg( VERBOSE_INFO | CLEAR_LINE,
							"%s file %s, sector %d: XCD deblocking error\n",
							( rfnum.type > DUMMY ) ? "target" : "source", filename,
							fileblk + start_blk + local_ready );
						for ( jx = start_blk; jx < ( start_blk + local_ready ); jx++ )
							loop_map[ jx ].type = GOOD;
						loop_map[ start_blk + local_ready ].type = ROUGH;
						local_ready++;	// to jump over rough sector
					} else {
						for ( jx = start_blk; jx < ( start_blk + local_ready ); jx++ )
							loop_map[ jx ].type = GOOD;
					}
					tbuf += local_ready * rrchain->max_blklen; tptr += local_ready;
					local_blks -= local_ready;
					start_blk += local_ready;
				}
			} else {	// blklen != XCD_LEN
				char *tbuf = buf;
				int start_blk = 0;
				int local_blks = loop_blks_read, local_good;
				if ( file->native_blklen != rrchain->max_blklen ) {
					char *tptr = readptr;
					for ( jx = 0; jx < loop_blks_read;
						jx++, tbuf += rrchain->max_blklen, tptr += blklen ) {
						memcpy( tbuf, tptr, blklen );
						memset( tbuf + blklen, 0, rrchain->max_blklen - blklen );
					}
				}
// рассматриваем блоки из сплошных нулей как битые:
				tbuf = buf;
				while ( local_blks > 0 ) {
					local_good = ( /*( file->need_flags & NO_RRPROCESS ) ||*/
							!( file->need_flags & ZERO_DAMAGED ) ) ? local_blks :
						rr_goodscan( tbuf, file->native_blklen,
							rrchain->max_blklen, local_blks );

					if ( local_good < local_blks ) {
						if ( local_good < 0 )
							local_good = 0;
						if ( errcount ) (*errcount)++;
						rr_printmsg( VERBOSE_NORMAL | CLEAR_LINE,
							"%s file %s, sector %d: is possibly damaged\n",
							( rfnum.type > DUMMY ) ? "target" : "source", filename,
							fileblk + start_blk + local_good );
						for ( jx = start_blk; jx < ( start_blk + local_good ); jx++ )
							loop_map[ jx ].type = GOOD;
						loop_map[ start_blk + local_good ].type = ZEROFILLED;
						local_good++;
					} else {
						for ( jx = start_blk; jx < ( start_blk + local_good ); jx++ )
							loop_map[ jx ].type = GOOD;
					}
					tbuf += local_good * rrchain->max_blklen;
					local_blks -= local_good;
					start_blk += local_good;
				}
/*				for ( jx = 0; jx < loop_blks_read; jx++ ) {
					loop_map[ jx ].type = GOOD;
				}*/
			}
// rr_makecopy копирует xcd в xcd без чистки:
			if ( rfnum.type /*< DUMMY*/ & NOTCOPIED ) {
				int jt = rr_makecopy( rrchain, rfnum, fileblk, loop_blks_read, buf,
						loop_map, &error );
				if ( jt < NO_ERROR ) {
//					if ( total_blks_read > 0 )
//						goto ok;
					if ( errcount ) (*errcount)++;
					rc = jt; goto err1;
				}
/*
				if ( file->out_handle != -1 ) {	// отметим, что уже есть копия
					for ( jt = 0; jt < loop_blks_read; jt++ )
						if ( loop_map[ jt ].type > DUMMY )
						rrchain->blk_map[ fileblk + file->blk_from + jt ].type = GOOD;
// перенесено в rr_makecopy:
					if ( file->out_blks < ( fileblk + loop_blks_read ) )
						file->out_blks = fileblk + loop_blks_read;
				}
*/
			}
// %%% доделать quick hack - сохранять битый пакет в копию: чистить сектор
			fileblk += loop_blks_read;
			total_blks_read += loop_blks_read;
			buf += rrchain->max_blklen * loop_blks_read;
			if ( endloop_flag )
				goto ok2;
			assert( loop_blks_read == loopblkcount );
			fileblkcount -= loopblkcount;
		}
//		if ( rfnum.type == UNTOUCHED )
//			file->pos_bytes = _lseek( handle, 0, SEEK_CUR );
	}
	rc = NO_ERROR;
ok:
ok2:
	rrchain->pos_blks += total_blks_read;
//	rrchain->error = error;
	rrchain->last_error = 0;
	if ( rc >= NO_ERROR )
		return ( total_blks_read );
// это всё работает, только когда были ошибки:
	rc = total_blks_read;
//	rrchain->pos_blks++;
//	printf( "%d blocks read\n", total_blks_read );
#ifndef RR_FILL_BAD
	rrchain->pos_blks--;	// для компенсации следующей строки
err1:
	memset( buf, 0, rrchain->max_blklen ); rrchain->pos_blks++;
	if ( ( rc > FATAL_ERROR ) && ( rfnum.type /*< DUMMY*/ & NOTCOPIED ) ) {	// записать в файл-копию
		int jt;
//		sprintf( buf, "=== BADBLOCK #%06d ===", rrchain->pos_blks - 1 );
		if ( ( jt = rr_makecopy( rrchain, rfnum, fileblk, 1, NULL,
						NULL, &error ) ) < 0 )
			rc = jt;
//		memset( buf, 0, OGM_LEN );
	}
#else /* RR_FILL_BAD */
err1:
#endif /* RR_FILL_BAD */
//	total_blks_read++; buf += OGM_LEN;
err2:
	rrchain->last_error = error;
	errno = 0;
	return rc;
}

int rr_write( rrchain_t *rrchain, char *buf, int blkcount ) {
	int
	jt,
//		filenum,
	handle,
	fileblk;			// номер начального блока в файле
	rritem_t *file;
	rrmap_t rfnum;
	char *decoy_ptr;
	if ( blkcount != 1 ) {
		rr_printmsg( VERBOSE_QUIET,
			"Unsupported parameter: ( blkcount == %d ) != 1\n", blkcount );
		return ( FATAL_ERROR );
	}
	rfnum = rrchain->blk_map[ rrchain->pos_blks ];
	if ( rfnum.type & DUMMY )
		return ( blkcount );
//	if ( rfnum > 0 )
//		rfnum = -rfnum;
//	filenum = rfnum.rfile;
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	file = rfnum.rritem;
	if ( ( jt = rr_select( rrchain, file, NULL ) ) < SOFT_ERROR )
		return ( jt );
	handle = rrchain->active_out_handle;
	fileblk = rrchain->pos_blks - file->blk_from;
	if ( file->need_flags & DECOY_ITEM ) {
		decoy_ptr = file->decoy_buf + fileblk * file->native_blklen;
		memcpy( decoy_ptr, buf, file->native_blklen );
	} else {
		if ( handle == -1 )
			return ( /*SOFT_ERROR*/ blkcount );
		if ( ( jt = rr_seekcopy( rrchain, rfnum, fileblk, &rrchain->last_error ) )
			< NO_ERROR )
			return ( jt );
		if ( ( jt = rr_makecopy( rrchain, rfnum, fileblk, blkcount, buf, NULL,
						&rrchain->last_error ) ) < NO_ERROR )
			return ( jt );
	}
	rrchain->blk_map[ rrchain->pos_blks ].type = GOOD;
	return ( blkcount );
}

int rr_tell( rrchain_t *rrchain ) {
	return ( rrchain->pos_blks );
}
int rr_seek( rrchain_t *rrchain, int block ) {
	if ( block < 0 ) block = 0;
	if ( block > rrchain->total_blks ) block = rrchain->total_blks;
	rrchain->pos_blks = block;
	return block;
}
int rr_last_error( rrchain_t *rrchain ) {
	return ( rrchain->last_error );
}
int rr_errcount( rrchain_t *rrchain ) {
	int jx, jy;
	for ( jx = 0, jy = 0; jx < rrchain->total_blks; jx++ )
		if ( rrchain->blk_map[ jx ].type /*< DUMMY*/ & NOTCOPIED )	// не скопировался
			jy++;
	return ( jy );
}
int *rr_errlocs( rrchain_t *rrchain ) {
	int jx, jy;
	int *ret = rrchain->err_locs;
	for ( jx = 0, jy = 0; jx < rrchain->total_blks; jx++ ) {
		if ( rrchain->blk_map[ jx ].type /*< DUMMY*/ & NOTCOPIED ) {	// не скопировался
			if ( jy >= rrchain->err_limit )
				return ( NULL );
			ret[ jy++ ] = jx;
		}
	}
	ret[ jy ] = -1;
	return ret;
}
int rr_close( rrchain_t *rrchain ) {
	return rr_finish( rrchain, 0 );
}
int rr_getbufsize( rrchain_t *rrchain ) {
	return ( rrchain->read_blks );
}
void rr_setbufsize( rrchain_t *rrchain, int size ) {
	if ( ( size < 0 ) || ( size > XCD_BUFBLKS ) ) size = XCD_BUFBLKS;
	else if ( size < 1 ) size = 1;
	rrchain->read_blks = size;
}
int rr_untouched( rrchain_t *rrchain, int pos ) {
	int jx, // jt,
	badmask = ( rrchain->read_blks > 1 ) ?
		( ROUGH | UNTOUCHED ) : ( SHADY | ROUGH | UNTOUCHED );
//		mintype = ( rrchain->read_blks > 1 ) ? ROUGH : SHADY;
	for ( jx = pos; jx < rrchain->total_blks; jx++ ) {
//		jt = rrchain->blk_map[ jx ].type;
//		if ( ( jt /*< DUMMY*/ & NOTCOPIED ) && ( jt >= mintype ) )
		if ( rrchain->blk_map[ jx ].type & badmask )
			return ( jx );
	}
	return ( -1 );
}
int rr_typelen( rrchain_t *rrchain, int pos, int limit ) {
	int jx, maxret = rrchain->total_blks - pos;
	rrmap_t type = rrchain->blk_map[ pos ];
	if ( maxret < limit ) {
//		return ( 1 );
		limit = maxret;
	}
	for ( jx = /*pos +*/ 1; jx < /*rrchain->total_blks*/ limit; jx++ ) {
		if ( !RRMAP_EQ( type, rrchain->blk_map[ pos + jx ] ) )
			return ( jx /*- pos*/ );
	}
//	return ( 1 );
	return ( /*rrchain->total_blks - pos*/ limit );
}
int rr_used_rrcount( rrchain_t *rrchain, int copied_only ) {
	int jx, jr = 0, js, jy, jm = copied_only ? NOTCOPIED : HAS_ERRORS;
	int *count = copied_only ? &rrchain->untouched_sectors : &rrchain->bad_sectors;
	if ( *count >= 0 ) {
		jr = *count;
	} else {
		for ( js = 0; js < RR_STREAMNUM; js++ ) {
			for ( jx = js, jy = 0; jx < rrchain->total_blks; jx += RR_STREAMNUM )
				if ( rrchain->blk_map[ jx ].type /*< DUMMY*/ & jm )	// не скопировался
					jy++;
			if ( jy > jr )
				jr = jy;
		}
		*count = jr;
	}
	return ( jr );
}
int rr_closein( rrchain_t *rrchain ) {
	int jx, rc = 0;
	struct list_head *jp;
	rr_select( rrchain, NULL, NULL );	// закроем всё
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
list_for_each( jp, &rrchain->items_list ) {
	rritem_t *file = list_entry( jp, rritem_t, list_item );
	if ( file->write_allow /* && file->read_allow */ )
		file->read_allow = 0;
	else if ( !( file->need_flags & DECOY_ITEM ) )
		rc++;
#ifdef RR_CUT_COPY
	if ( file->write_allow ) {
		rr_shape_outfile( file, NULL, 0 );
	}
#endif /* RR_CUT_COPY */
}
for ( jx = 0; jx < rrchain->total_blks; jx++ ) {
	rrmap_t *blk = rrchain->blk_map + jx;
	if ( blk->type /*< DUMMY*/ & NOTCOPIED )
		blk->type = NOTREADABLE;
}
return ( rc );
}
/*
int rr_savelist(	// ВНИМАНИЕ! Функция заточена под CD-ROM (блоки 2048 и 2324)
		rrchain_t *rrchain, FILE *outfile ) {
//	char buf[ 3 * _MAX_PATH ];
	char buf1[ _MAX_DIR ] = "", buf2[ _MAX_DIR ] = "",
		*dir1 = buf1, *dir2 = buf2;
	int len, fulllen, rc;
	struct list_head *jp;
	fulllen = fprintf( outfile, "%d\n", rrchain->rrcount );
	if ( ferror( outfile ) ) {
err:
		rc = errno;
		printf( "filelist: write failed: %d - %s\n",
				rc, strerror( rc ) );
		return ( -rc );
	}
	list_for_each( jp, &rrchain->items_list ) {
		rritem_t *file = list_entry( jp, rritem_t, list_item );
		int dirlen;
		char *fname = ( rrchain->rrheader == file ) ?
			file->out_name : file->native_name;
		char lenmode = ( file->native_blklen == ISO_LEN ) ? 'i' : 'x';
		_splitpath( fname, NULL, dir1, NULL, NULL );
		if ( ( dirlen = strlen( dir1 ) ) > 1 ) {
			char *dirend = dir1 + dirlen - 1;
			if ( *dirend == '\\' )
				*dirend = '\0';
		}
		if ( strcmp( dir1, dir2 ) ) {	// ё-пс! смена каталога
			char *tdir = dir1;
			fprintf( outfile, "-d %s\n", dir1 );
			strcpy( dir2, dir1 );
			dir1 = dir2; dir2 = tdir;
		}
		if ( file->need_flags & NEED_ALIGN )
			lenmode = toupper( lenmode );
		len = fprintf( outfile, "-%c %d %d %s\n", lenmode,
			(int) file->filesize, file->filetime, fname );
		if ( ferror( outfile ) )
			goto err;
		fulllen += len;
	}
	return ( fulllen );
}
 */
int rr_readiness( rrchain_t *rrchain ) {
	return ( ( rrchain->pos_blks * FULL_READINESS ) / rrchain->total_blks );
}
const char *rr_srcbase( rrchain_t *rrchain ) {
	return ( rrchain->src_base );
}
const char *rr_tgtbase( rrchain_t *rrchain ) {
	return ( rrchain->tgt_base );
}

int rr_lastfile_size( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( SOFT_ERROR );
	return ( list_entry(rrchain->items_list.prev, rritem_t,
			list_item )->filesize );
}
time_t rr_lastfile_time( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( SOFT_ERROR );
	return ( list_entry(rrchain->items_list.prev, rritem_t,
			list_item )->filetime );
}
int rr_lastfile_blklen( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( SOFT_ERROR );
	return ( list_entry(rrchain->items_list.prev, rritem_t,
			list_item )->native_blklen );
}
/*
int rr_lastfile_align( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( SOFT_ERROR );
	return ( ( list_entry(rrchain->items_list.prev, rritem_t,
		list_item )->need_flags & NEED_ALIGN ) != 0 );
}
int rr_lastfile_norrprocess( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( SOFT_ERROR );
	return ( ( list_entry(rrchain->items_list.prev, rritem_t,
		list_item )->need_flags & NO_RRPROCESS ) != 0 );
}
*/
int rr_lastfile_flags( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( SOFT_ERROR );
	return ( list_entry(rrchain->items_list.prev, rritem_t,
			list_item )->need_flags );
}
int rr_lastfile_tell( rrchain_t *rrchain ) {
//	rritem_t *it;
	if ( list_empty( &rrchain->items_list ) )
		return ( SOFT_ERROR );
	return ( list_entry(rrchain->items_list.prev, rritem_t,
			list_item )->blk_from );
}
#ifndef RR_UPDATE_DIR
const char *rr_lastfile_full_in_name( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( NULL );
	return ( list_entry(rrchain->items_list.prev, rritem_t,
			list_item )->full_in_name );
}
const char *rr_lastfile_full_out_name( rrchain_t *rrchain ) {
	if ( list_empty( &rrchain->items_list ) )
		return ( NULL );
	return ( list_entry(rrchain->items_list.prev, rritem_t,
			list_item )->full_out_name );
}
#else /* RR_UPDATE_DIR */
const char *rr_full_in_name( rritem_t *file ) {
	return ( ( file == NULL ) ? NULL : file->full_in_name );
}
const char *rr_full_out_name( rritem_t *file ) {
	return ( ( file == NULL ) ? NULL : file->full_out_name );
}
#endif /* RR_UPDATE_DIR */

int rr_blk_good( rrchain_t *rrchain, int blknum ) {
	if ( ( blknum < 0 ) || ( blknum >= rrchain->total_blks ) )
		return ( SOFT_ERROR );
	return ( !!( rrchain->blk_map[ blknum ].type == GOOD ) );
}
int rr_corrupt_blks( rrchain_t *rrchain, int blk_from, int blk_to ) {
	int jx;
	if ( ( blk_from < 0 ) || ( blk_from > blk_to ) ||
		( blk_to >= rrchain->total_blks ) )
		return ( SOFT_ERROR );
	for ( jx = blk_from; jx <= blk_to; jx++ )
		rrchain->blk_map[ jx ].type = BAD;
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	return ( NO_ERROR );
}
int rr_dummy_blk( rrchain_t *rrchain, int blknum ) {
	if ( ( blknum < 0 ) || ( blknum >= rrchain->total_blks ) )
		return ( SOFT_ERROR );
	rrchain->blk_map[ blknum ].type = DUMMY;
	rrchain->bad_sectors = rrchain->untouched_sectors = -1;
	return ( NO_ERROR );
}
/* было для RR_PROBE_MODE
int rr_blk_tag( rrchain_t *rr, int blknum, rrblktag_t *tag ) {
	rritem_t *file;
	if ( ( blknum < 0 ) || ( blknum >= rr->total_blks ) )
		return ( SOFT_ERROR );
	if ( tag != NULL ) {
		file = rr->blk_map[ blknum ].rritem;
		tag->filename = file->in_name;
		tag->sector = blknum - file->blk_from;
		tag->blksize = file->native_blklen;
		tag->offset = file->in_skip_bytes + tag->sector * file->in_blklen +
			file->in_skip_per_blk;
	}
	return ( NO_ERROR );
}
*/
int rr_files( rrchain_t *rrchain ) {
	return ( rrchain->total_files );
};
int rr_blks( rrchain_t *rrchain ) {
	return ( rrchain->total_blks );
}
#ifdef RR_DMG_ENTRIES
int rr_byte2blk( rritem_t *file, size_t pos ) {	// какому блоку принадлежит байт
	return ( ( file == NULL ) ? FATAL_ERROR :
		( ( ( pos < 0 ) || ( pos >= file->len_bytes ) )
			? -1 : ( ( pos < file->in_skip_bytes ) ? 0 :
				( ( pos - file->in_skip_bytes ) / file->native_blklen ) ) ) );
}
int rr_get1st_blk( rritem_t *file ) {	// с какого блока начинается файл
	return ( file->blk_from );
}
int rr_getblks( rritem_t *file ) {	// сколько блоков в файле
	return ( file->len_blks );
}
rrchain_t *rr_getchain( rritem_t *file ) {
	return ( file->rrchain );
}

#endif /* RR_DMG_ENTRIES */
