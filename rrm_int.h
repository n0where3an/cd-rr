#ifndef _RRM_INT_H_
#define _RRM_INT_H_

#include "rrmath.h"

//#define RR_NO_TABLE	/* обойдёмся без табличной математики */
//#define RRDEBUG

#ifdef RR_NO_TABLE
//	typedef unsigned int rrmath_t;	// тип внутренних данных
	#define PAGE_OFFSET		10	/* 2^10 int'ов на страницу */
	#define LINE_OFFSET		4		/* 2^4 int'ов на строку кэша */
	#define INDEX_OFFSET( index ) \
		( ( (index) << PAGE_OFFSET ) + ( (index) << LINE_OFFSET ) )
	#define PARTS( index1, index2 ) \
		jp[ INDEX_OFFSET( index1 ) + (index2) ]
#define INDEX_MASK( type, offset ) \
	( ( (type) 1 << (offset) ) - 1 )
#define PAGE_MASK \
	INDEX_MASK( unsigned long, PAGE_OFFSET + 2 )
#define LINE_MASK \
	INDEX_MASK( rrcode_t, LINE_OFFSET )
#define PAGE_ALIGNED( type, name, pages ) \
	/* PAGE_ALIGNED( type, name, pages ); */ \
	char name ## _base[ ( (pages) + 1 ) << ( PAGE_OFFSET + 2 ) ]; \
	type *name = (type *) ( ( (unsigned long) name ## _base + \
	PAGE_MASK ) & ~PAGE_MASK )
#endif /* RR_NO_TABLE */

//#define RR_BIGTABLE
//#define RRMULTI
#define RR_ALIGN 64

#define RR_MAGIC 45
#define RR_BASELEN /* 8 */ ( 8 * sizeof( rrdata_t ) )
//#define RR_BASELEN /* 8 */ 20
//#define RR_BASELEN /*12*/ 16

#define RR_BYTELEN ( 8 * sizeof( rrdata_t ) )
#define RR_HALFLEN ( RR_BASELEN / 2 )

#define RR_MAXBYTE /* 256 */ ( 1 << RR_BASELEN )
#define RR_MAXHALF /* 256 */ ( 1 << RR_HALFLEN )
#define RR_HALFMASK /* 256 */ ( RR_MAXHALF - 1 )

#define MAXPAR /*255*/ ( RR_MAXBYTE - 1 )
#define MAXPOL /*512*/ ( 2 * RR_MAXBYTE )

#ifdef RRDEBUG
//      #define HEXOUT "%04x"
//      #define EMPTY "----"
	#define HEXOUT "%04x"
	#define EMPTY "----"
#endif /* RRDEBUG */

//typedef int bool; 

typedef int rrcode_t;
//typedef rrdata_t rrcode_t;

typedef rrcode_t *rrbuf_t;

struct rrtable_s {
	int magic;
	int rrcount;
	int baselen;
	int blksize;            // размер блока секторов (в rrdata_t'ах)
//      rrbuf_t *buf;
	rrbuf_t *rrdata;        // [ blksize ][ rrcount ]
	rrbuf_t *syndata;       // [ rrcount ][ blksize ]
	#define RR_ISSERO1( a )                 ( (a) == 0 ) ? 0 :
	#define RR_ISSERO2( a, b )      ( (a) == 0 || (b) == 0 ) ? 0 :
	#define RR_ISSERO3( a )                 ( (a) < 0 ) ? 0 :
	rrcode_t gfExp[ MAXPOL ];
	rrcode_t gfLog[ MAXPAR + 1 ];
	rrcode_t GXPol[ /*MAXPOL * 2*//*RR_MAXBYTE*/ MAXPOL ];
//      rrcode_t *gfExp;
//#define gfExp (full_exp+MAXPOL)
#ifndef RR_BIGTABLE
	unsigned int poly[ RR_MAXHALF ];
#else /* RR_BIGTABLE */
	unsigned int *poly;
	unsigned int multtbl[ RR_MAXBYTE ][ RR_MAXHALF ];
#endif /* RR_BIGTABLE */
};

//#include "rr.h"
#include "rrmath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <winsock.h>
#define RR_INLINE
#ifdef RRDEBUG
extern __int64 cc1, cc2;
#endif /* RRDEBUG */
//#define RR_WHILE
#undef RR_FORLOOP
#define RR_BLOCKLOOP
#define RR_NEG_OFFSET
//#define RR_COUNT
//#define RR_GFMACRO
//#define RR_SWAP
// %%% при включенном съезжает SynData!
//#define RR_MULTDEMO

//#define Clean(D,S)  {int I; for (I=0;I<(S);I++) (D)[I]=0;}
#define Clean(D,S)  do{memset((D),0,sizeof((D)[0])*(S));}while(0)
#ifndef __GCC__
#pragma warning( disable :4127)
#pragma warning( disable :4711)
#pragma warning( disable :4702)
#endif

#ifdef RR_COUNT
	#define RR_CLR( a ) (a) = 0
	#define RR_INC( a ) (a)++
	#define RR_ADD( a, b  ) (a) += (b)
#else /* RR_COUNT */
	#define RR_CLR( a )
	#define RR_INC( a )
	#define RR_ADD( a, b  )
#endif /* RR_COUNT */

#ifdef RR_SWAP
	#define RR_EXTRALINE 1
#else /* RR_SWAP */
	#define RR_EXTRALINE 0
#endif /* RR_SWAP */

rrbuf_t *rr_getbuf( int height, int width, int extra );
void rr_pnmult( const rrtable_t *rr, rrcode_t *p1, rrcode_t *p2, rrcode_t *r );

#ifdef RRDEBUG
//      #define HEXOUT "%04x"
//      #define EMPTY "----"
	#define HEXOUT "%04x"
	#define EMPTY "----"
	void printtbl( char *title, rrcode_t *tbl, int size );
#else /* RRDEBUG */
	#define printtbl( a, b, c )
#endif /* RRDEBUG */

#ifdef RR_NO_TABLE
	void rr_gentable_( rrcode_t *table, rrcode_t b );
	#define rr_gentable( table, b ) rr_gentable_( table, b ) 

	__inline rrcode_t rr_mult( const rrtable_t *rr, 
			rrcode_t a, const rrcode_t *jp ) {
		rrcode_t part, ret;

		part = a & LINE_MASK; a >>= LINE_OFFSET;
		ret = PARTS( 0, part );
		part = a & LINE_MASK; a >>= LINE_OFFSET;
		ret ^= PARTS( 1, part );
		part = a & LINE_MASK; a >>= LINE_OFFSET;
		ret ^= PARTS( 2, part );
		part = a & LINE_MASK; a >>= LINE_OFFSET;
		ret ^= PARTS( 3, part );
		part = a & LINE_MASK; a >>= LINE_OFFSET;
		ret ^= PARTS( 4, part );
		part = a & LINE_MASK; a >>= LINE_OFFSET;
		ret ^= PARTS( 5, part );
		part = a & LINE_MASK; a >>= LINE_OFFSET;
		ret ^= PARTS( 6, part );
//		part = a & LINE_MASK; a >>= LINE_OFFSET;
//		ret ^= PARTS( 7, part );
		return ( ret ^ PARTS( 7, a ) );
	}
#else /* RR_NO_TABLE */
	#ifndef RR_BIGTABLE
		void rr_gentable_( unsigned int *table, unsigned int b );
		#define rr_gentable( table, b ) rr_gentable_( table, b ) 
		#ifdef RR_INLINE
		#ifndef __GNUC__
		__inline static
		#else
		inline static
		#endif
		#endif /* RR_INLINE */
			unsigned int rr_mult( const rrtable_t *rr, 
				unsigned int a, const unsigned int *jp ) {
			unsigned int ret = 0;
			unsigned int jx, jy;
		#ifdef RR_MULTDEMO
			return ( a ^ jp[ 0 ] );
		#endif /* RR_MULTDEMO */
	//              jy = a & RR_HALFMASK;

		#if 0
			#if 1
			jy = a & RR_HALFMASK;
			if ( a ) do {
				jx = jp[ jy ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				jp = rr->poly; a = jx >> RR_BASELEN; ret ^= jx; jy = a & RR_HALFMASK;
			} while ( a );
			#else
				jy = a & RR_HALFMASK;
				jx = jp[ jy ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				a = jx >> RR_BASELEN; ret ^= jx; jy = a & RR_HALFMASK;
				jx = rr->poly[ jy ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				a = jx >> RR_BASELEN; ret ^= jx; jy = a & RR_HALFMASK;
				jx = rr->poly[ jy ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				ret ^= jx; 
			#endif
		#else
			#if 0
				jy = a & RR_HALFMASK;
				jx = jp[ jy ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				jp = rr->poly; a = jx >> RR_BASELEN; ret ^= jx; 
	//                      if ( a == 0 ) return ( ret & MAXPAR );
				jy = a & RR_HALFMASK;
				jx = jp[ jy ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				jp = rr->poly; a = jx >> RR_BASELEN; ret ^= jx; 
	//                      if ( a == 0 ) return ( ret & MAXPAR );
				jy = a & RR_HALFMASK;
				jx = jp[ jy ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				jp = rr->poly; a = jx >> RR_BASELEN; ret ^= jx; // jy = a & RR_HALFMASK;
			#else
	#define RRMULT_STEP_02( ptr ) \
				do { \
					jx = ( (ptr)[ a >> RR_HALFLEN ] << RR_HALFLEN ) ^ (ptr)[ a & RR_HALFMASK ]; \
					a = jx >> RR_BASELEN; ret ^= jx; \
				} while(0)

	#define RRMULT_STEP( ptr ) \
				do { \
					register int j1, j2, j3, j4; \
					j1 = a >> RR_HALFLEN; \
					jy = a & RR_HALFMASK; \
					j2 = (ptr)[ j1 ]; \
					j4 = (ptr)[ jy ]; \
					j3 = j2 << RR_HALFLEN; \
					jx = j4 ^ j3; \
					/* jp = rr->poly; */ \
					a = jx >> RR_BASELEN; \
					ret ^= jx; \
				} while(0)
				{
					RRMULT_STEP( jp );
					RRMULT_STEP( rr->poly );
					RRMULT_STEP( rr->poly );
				}
	/*
	//                      jy = a & RR_HALFMASK;
	//                      jx = jp[ a & RR_HALFMASK ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
				a = ( jx = jp[ a & RR_HALFMASK ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN ) ) >> RR_BASELEN; 
				ret ^= jx; 
	//                      jp = rr->poly; 
	//                      if ( a == 0 ) return ( ret & MAXPAR );
	//                      jy = a & RR_HALFMASK;
	//                      jx = rr->poly[ a & RR_HALFMASK ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
	//                      jp = rr->poly; 
				a = ( jx = rr->poly[ a & RR_HALFMASK ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN ) ) >> RR_BASELEN; 
				ret ^= jx; 
	//                      if ( a == 0 ) return ( ret & MAXPAR );
	//                      jy = a & RR_HALFMASK;
				jx = rr->poly[ a & RR_HALFMASK ] ^ ( jp[ a >> RR_HALFLEN ] << RR_HALFLEN );
	//                      jp = rr->poly; 
	//                      a = jx >> RR_BASELEN; 
				ret ^= jx; 
	//                      jy = a & RR_HALFMASK;
	*/
			#endif
		#endif
			return ( ret & MAXPAR );
		}
	#else /* RR_BIGTABLE */
		#define rr_gentable( table, b ) \
			do { \
				(table) = rr->gfExp + rr->gfLog[ b ]; \
				/*(table) = &rr->multtbl[ (b) ][ 0 ];*/ \
				/*(table) = &rr->multtbl[ 1 ][ 0 ];*/ \
			} while (0)
	#ifdef RR_INLINE
		__inline 
	#endif /* RR_INLINE */
			unsigned int rr_mult( const rrtable_t *rr, 
				unsigned int a, const unsigned int *jp ) {
			return ( ( a==0 ) ? 0 :
				jp[ rr->gfLog[ a ] ] );
		}
	#endif /* RR_BIGTABLE */
#endif /* RR_NO_TABLE */

#if 0
void gfInit( rr_t *rr );
int gfMult( const rr_t *rr, int a, int b );
void pnInit( rr_t *rr );
void pnMult( const rr_t *rr, int *p1, int *p2, int *r );
void rr_magic( rr_t *rr, int magic );
//bool Decode( rr_t *rr, rrdata_t *Data, int DataSize, int *EraLoc, int EraSize );
int rr_pump(    // "накачка" ремонтных данных одним сектором
		const rrtable_t *table, rrdata_t *data, // сектор
		int size, int count     // размер сектора и кол-во секторов
		);
#endif /*0 */

#endif /* _RRM_INT_H_ */
