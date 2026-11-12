#include "rrm_int.h"

int // кол-во выведенных секторов
rr_dump(	// "сброс" ремонтных данных в ремонтный сектор
		/*const*/ rrtable_t *rr, 
		int blk_count, // кол-во секторов
		int num,		// номер первого ремонтного сектора
		rrdata_t *data	// собственно данные
		) {
	rrbuf_t *rrbuf = rr->rrdata;
	int jx/*, jy, jc = rr->rrcount - 1*/;
	if ( rrbuf == NULL )
		return -1;
	if ( ( num + blk_count ) > rr->rrcount )
		blk_count = rr->rrcount - num;
	for ( jx = 0; jx < rr->blksize; jx++ ) {
		int jn;
		rrcode_t *js = rrbuf[ jx ];
		for ( jn = num; jn < ( num + blk_count ); jn++ ) {
			data[ jn * rr->blksize + jx ] = (rrdata_t) js[ rr->rrcount - jn - 1 ];
		}
		printtbl( "ShiftReg", js, rr->rrcount );
	}
//	printtbl( "recdata", data, blk_count * rr->blksize / 2 );
	rr->rrdata = NULL;
	free( rrbuf - RR_EXTRALINE );
	return ( blk_count );
}
/*
int rr_prefetch( void *p ) {
	__asm {
		mov eax, p
		prefetchnta [eax]
	}
}
*/
int rr_encode( /*const*/ rrtable_t *rr, 
//		int blk_size, 
		int blk_count, rrdata_t *Data /*, int DataSize, int *DestData */ ) {
//  int js[ MAXPAR + 1 ];
	int I, /*J,*/ offset;
//	int jc = rr->rrcount - 1;
#ifdef RR_NO_TABLE
	PAGE_ALIGNED( rrcode_t, jm, 8 );
#else /* RR_NO_TABLE */
	#ifndef RR_BIGTABLE
		#ifndef RR_ALIGN
			unsigned int jm[ RR_MAXHALF ] = { 0, 0 };
		#else /* RR_ALIGN */
			unsigned int jm_[ RR_MAXHALF + RR_ALIGN / sizeof( unsigned int ) ];
			unsigned int *jm = (unsigned int *) 
				( (unsigned long) ( (char *) jm_ + ( RR_ALIGN - 1 ) ) & ~( RR_ALIGN - 1 ) );
			jm[ 0 ] = jm[ 1 ] = 0;
		#endif /* RR_ALIGN */
	#else /* RR_BIGTABLE */
		unsigned int *jm;
	#endif /* RR_BIGTABLE */
#endif /* RR_NO_TABLE */
	RR_CLR( cc1 ); RR_CLR( cc2 );

	if ( rr->rrdata == NULL )
		if ( ( rr->rrdata = rr_getbuf( rr->blksize, rr->rrcount,
			RR_EXTRALINE ) ) == NULL )
			return -1;

	for ( offset = 0; offset < rr->blksize; offset++ ) {
		rrcode_t *js = rr->rrdata[ offset ];
//#pragma prefetch js,rr->blksize
#ifndef RR_SWAP
	#define js2 js
#else /* RR_SWAP */
		rrcode_t *js2 = rr->rrdata[ -1 ];
#endif /* RR_SWAP */
		for ( I = 0; I < blk_count; I++ ) {
			register int J = rr->rrcount - 1; 
			int D = Data[ I * rr->blksize + offset ] ^ js[ J ];
			if ( D != 0 ) {
				rr_gentable( jm, D );
#ifdef RR_FORLOOP
				for ( /*J = rr->rrcount - 1*/; J > 0; J-- ) {
					js2[ J ] = js[ J - 1 ] ^ rr_mult( rr, rr->GXPol[ J ], jm );
				}
#else /* RR_FORLOOP */
#ifdef RR_BLOCKLOOP
				if ( J >= 4 ) do {
//					rr_prefetch( js + J - 20 );
//					void *p = js + J - 20;
//					void *pp = &p;
//					__asm mov eax, p
//					__asm prefetchnta [eax]
					RR_ADD( cc1, 4 );
#ifndef RR_NEG_OFFSET
					js2[ J     ] = js[ J - 1 ] ^ rr_mult( rr, rr->GXPol[ J     ], jm );
					js2[ J - 1 ] = js[ J - 2 ] ^ rr_mult( rr, rr->GXPol[ J - 1 ], jm );
					js2[ J - 2 ] = js[ J - 3 ] ^ rr_mult( rr, rr->GXPol[ J - 2 ], jm );
					js2[ J - 3 ] = js[ J - 4 ] ^ rr_mult( rr, rr->GXPol[ J - 3 ], jm );
					J -= 4;
#else /*RR_NEG_OFFSET*/
					J -= 4;
//#pragma prefetch js[ J - 20 ]
					js2[ J + 4 ] = js[ J + 3 ] ^ rr_mult( rr, rr->GXPol[ J + 4 ], jm );
					js2[ J + 3 ] = js[ J + 2 ] ^ rr_mult( rr, rr->GXPol[ J + 3 ], jm );
					js2[ J + 2 ] = js[ J + 1 ] ^ rr_mult( rr, rr->GXPol[ J + 2 ], jm );
					js2[ J + 1 ] = js[ J     ] ^ rr_mult( rr, rr->GXPol[ J + 1 ], jm );
#endif /*RR_NEG_OFFSET*/
				} while ( J >= 4 );
				if ( J > 0 )
#endif /* RR_BLOCKLOOP */
				do {
					RR_INC( cc1 );
					js2[ J ] = js[ J - 1 ] ^ rr_mult( rr, rr->GXPol[ J ], jm );
					J--;
				} while ( J > 0 );
#endif /* RR_FORLOOP */
				js2[ 0 ] = rr_mult( rr, rr->GXPol[ 0 ], jm );
			} else {
//				J = jc; 
#ifdef RR_BLOCKLOOP
				if ( J >= 4 ) do {
					RR_ADD( cc1, 4 );
					js2[ J     ] = js[ J - 1 ];
					js2[ J - 1 ] = js[ J - 2 ];
					js2[ J - 2 ] = js[ J - 3 ];
					js2[ J - 3 ] = js[ J - 4 ];
					J -= 4;
				} while ( J >= 4 );
				if ( J > 0 )
#endif /* RR_BLOCKLOOP */
				do {
					RR_INC( cc1 );
					js2[ J ] = js[ J - 1 ];
					J--;
				} while ( J > 0 );
				js2[ 0 ] = 0;
			}
#ifdef RR_SWAP
			{
				rrcode_t *jt = rr->rrdata[ -1 ] = js;
				rr->rrdata[ offset ] = js = js2;
				js2 = jt;
			}
#endif /* RR_SWAP */
		}
//		printtbl( "dtable", dtable, 1 << RR_BYTELEN );
//		printtbl( "rrtable", rrtable, RR_MAXBYTE );
	}
//	printtbl( "dtable", dtable, 1 << RR_BYTELEN );
//	printtbl( "rrtable", rrtable, RR_MAXBYTE );
	return blk_count;
}
