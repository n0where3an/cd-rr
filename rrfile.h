#ifndef __RRFILE_H__
#define __RRFILE_H__

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "rr.h"
#include "xcdhdrs.h"
#ifndef __GCC__
#pragma warning( disable: 4127 )
#endif
#ifdef	__cplusplus
extern "C" {
#endif

#define XCD_BUFBLKS /* 256 */ 32
//#define RR_STREAMNUM 6
#define MAX_BLKNUM /*400000*/ ( RR_STREAMNUM * 65535 )

// need_flags на входе:
#define NEED_ALIGN			0x0001
#define NO_SRCBASE			0x0002
#define NO_RRPROCESS		0x0004
#define NO_XCDDECODE		0x0008
#define NO_COPYPLAIN		0x0010
#define NO_FILELEN			0x0020
#define ZERO_DAMAGED		0x0040
#define ALLOW_OVERWRITE	0x0080
#define DECOY_ITEM			0x0100
#define RRLIST_ITSELF		0x0200
#define NO_STRICT				0x0400

#ifdef RR_KEEP_BAD
#define KEEP_BAD_FILE   0x0800
#endif /* RR_KEEP_BAD */
#ifdef RR_KEEP_EXISTING
#define KEEP_EXIST_FILE	0x1000
#endif /* RR_KEEP_EXISTING */
#ifdef RR_NO_INIT_READ
#define NO_INIT_READ		0x2000
#endif /* RR_NO_INIT_READ */


// need_flags на выходе:
#define XCD_INPUT				0x10000
#define XCD_OUTPUT			0x20000

#define PUT_NEED_APPEND	0x80000

#define RRNAME_LEN 11

typedef enum {
	RR_FOR_RECOVER,
	RR_FOR_COPY,
	RR_FOR_PROTECT
} rrpurpose_t;

//extern int ogm_len; 

#if 0
#pragma pack( 1 )
struct rrblock_s {
	unsigned char sync[ XCD_SYNC_LEN ];
	unsigned char header[ XCD_HDR_LEN ];
	unsigned char subheader[ XCD_SUBHDR_LEN ];
	unsigned char data[ XCD_DATA_LEN ];
	unsigned int edc;
};
#pragma pack()
#define XCD_LEN sizeof( rrblock_t )
#endif /*0*/
struct riffhdr_s; struct rrchain_s; struct rritem_s;

typedef struct rrblock_s rrblock_t;
typedef struct riffhdr_s riffhdr_t;
//typedef struct rrchain_s rrchain_t;
typedef struct rritem_s rritem_t;
typedef struct rrblktag_s rrblktag_t;

struct rrblktag_s {
	char *filename;
	int sector;
	int offset;
	int blksize;
};

rrchain_t *rr_create( const char *rrname, const char *xcd_ext,
		int errcount, const char *src_base, const char *tgt_base,
#ifdef RR_UPDATE_DIR
		const char *upd_base,
#endif /* RR_UPDATE_DIR */
#ifdef RR_ALLOW_ABSPATH
		int do_full,	// делать/не делать преобразование базовых путей до абсолютных
#endif /* RR_ALLOW_ABSPATH */
		verbose_t verbose );
//rrchain_t *rr_open( const char **fnames, int fcount, int errcount, 
//		const char **out_fnames );
rritem_t *rr_append(		// добавление файла в rr-цепочку
#ifdef RR_STORE_EDC
		struct list_head *all_files,
#endif /* RR_STORE_EDC */
		rrchain_t *rrchain,	// цепочка
		int seclen,						// длина сектора; == 0 - макс. длина в rchain'е
		const char *infile,		// имя входного файла (NULL - для спец.случаев)
		const char *outfile,	// имя выходного файла (NULL, если вых. файл не нужен)
		int filelen,	// длина файла (для восстановления из XCD) или -1 - не трогать
		int filetime,	// время модификации	
		int need_flags );		// надо ли выравнивать файл хвостом по границе кластера;
												// надо ли игнорировать source base;

int rr_discard( rrchain_t *rrchain );
int rr_files( rrchain_t *rrchain );

int rr_copyopen( rrchain_t *rr, const char **fnames, int fcount );
int rr_read( rrchain_t *rrchain, char *buf, int blkcount, int *errcount
#ifdef RR_SHOW_LOST
		, int *lostcount
#endif /* RR_SHOW_LOST */
		);
int rr_write( rrchain_t *rrchain, char *buf, int blkcount );
int rr_seek( rrchain_t *rrchain, int block );
int rr_tell( rrchain_t *rrchain );
int rr_check( rrchain_t *rrchain, int block, int blkcount );
int rr_errcount( rrchain_t *rrchain );
int *rr_errlocs( rrchain_t *rrchain );
int rr_blks( rrchain_t *rrchain );
int rr_blklen( rrchain_t *rrchain );
int rr_close( rrchain_t *rrchain );
int rr_closein( rrchain_t *rrchain );
int rr_last_error( rrchain_t *rrchain );
int rr_getbufsize( rrchain_t *rrchain );
void rr_setbufsize( rrchain_t *rrchain, int size );
int rr_untouched( rrchain_t *rrchain, int pos );
int rr_typelen( rrchain_t *rrchain, int pos, int limit );
int rr_used_rrcount( rrchain_t *rrchain, int copied_only );
// int rr_savelist( rrchain_t *rrchain, FILE *outfile );
void rr_print( rrchain_t *rrchain );
int rr_readiness( rrchain_t *rrchain );
rritem_t *rr_setrrdata( rrchain_t *rrchain, const char *prefixfile,
		const char *rrdatafile, int rrcount, int flags );
#ifndef RR_NEW_ENC
int rr_putrrdata( rrchain_t *rrchain, char *buf );
#endif /* RR_NEW_ENC */
const char *rr_srcbase( rrchain_t *rrchain );
const char *rr_tgtbase( rrchain_t *rrchain );

int rr_lastfile_size( rrchain_t *rrchain );
int rr_lastfile_time( rrchain_t *rrchain );
int rr_lastfile_blklen( rrchain_t *rrchain );
//int rr_lastfile_align( rrchain_t *rrchain );
//int rr_lastfile_norrprocess( rrchain_t *rrchain );
int rr_lastfile_flags( rrchain_t *rrchain );
int rr_lastfile_tell( rrchain_t *rrchain );

#ifndef RR_UPDATE_DIR
	const char *rr_lastfile_full_in_name( rrchain_t *rrchain );
	const char *rr_lastfile_full_out_name( rrchain_t *rrchain );
#else /* RR_UPDATE_DIR */
	const char *rr_full_in_name( rritem_t *file );
	const char *rr_full_out_name( rritem_t *file );
#endif /* RR_UPDATE_DIR */

int rr_blk_good( rrchain_t *rrchain, int blknum );
int rr_corrupt_blks( rrchain_t *rrchain, int blk_from, int blk_to );
int rr_dummy_blk( rrchain_t *rrchain, int blknum );
//int rr_blk_tag( rrchain_t *rr, int blknum, rrblktag_t *tag );

char *rr_decoy_buf( rrchain_t *rrchain );
int rr_decoy_len( rrchain_t *rrchain );
rritem_t *rr_add_decoy( rrchain_t *rrchain, int blknum, int seclen );

char *rr_fullname( char *outname, const char *base, const char *name,
		const char *ext, const char *type, int do_full );
#ifdef RR_DMG_ENTRIES
int rr_byte2blk( rritem_t *file, size_t pos );	// какому блоку принадлежит байт
int rr_get1st_blk( rritem_t *file );	// с какого блока начинается файл
int rr_getblks( rritem_t *file );	// сколько блоков в файле
rrchain_t *rr_getchain( rritem_t *file );
#endif /* RR_DMG_ENTRIES */

// Всякий мелкий утиль:
unsigned long build_edc( const void *in, unsigned from, unsigned upto );


#ifdef	__cplusplus
}
#endif
#endif /*__RRFILE_H__ */
