#include "rrm_int.h"
int rr_decode( /*const*/ rrtable_t *rr, 
		int blk_count,	// кол-во блоков на обработку
//		int num,				// номер первого обрабатываемого блока
		rrdata_t *Data			// собственно данные
//		int blk_total,	// сумма кол-ва блоков с данными и ремонтных 
//		int *EraLoc, int EraSize
		 ) {
//	int offset;
	int jc;
//	rrcode_t Sum;
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
	RR_CLR( cc1 );
	if ( rr->syndata == NULL )
		if ( ( rr->syndata = rr_getbuf( rr->rrcount, rr->blksize,
			RR_EXTRALINE ) ) == NULL )
			return -1;

	for ( jc = 0; jc < rr->rrcount; jc++ ) {
		rrcode_t *sum = rr->syndata[ jc ];
#ifndef RR_SWAP
	#define sum2 sum
#else /* RR_SWAP */
		rrcode_t *sum2 = rr->syndata[ -1 ];
#endif /* RR_SWAP */
//		int exp = rr->gfExp[ jc + 1 ];
//		int jy = rr->blksize * blk_count;
		int jb, jd;
		rr_gentable( jm, rr->gfExp[ jc + 1 ] );
//		if ( num == 0 ) {
//			Clean( sum, rr->blksize );
//		}
		jd = jb = 0; do {	// по блокам
			int jp = 0;
//			int *sum = rr->SynData[ jp ];
#ifdef RR_BLOCKLOOP
			if ( jp + 4 <= rr->blksize ) do {	// по позициям
				RR_ADD( cc1, 4 );
				sum[ jp     ] = Data[ jd     ] ^ rr_mult( rr, sum[ jp     ], jm );
				sum[ jp + 1 ] = Data[ jd + 1 ] ^ rr_mult( rr, sum[ jp + 1 ], jm );
				sum[ jp + 2 ] = Data[ jd + 2 ] ^ rr_mult( rr, sum[ jp + 2 ], jm );
				sum[ jp + 3 ] = Data[ jd + 3 ] ^ rr_mult( rr, sum[ jp + 3 ], jm );
				jp += 4; jd += 4;
			} while ( jp + 4 <= rr->blksize );
			if ( jp < rr->blksize ) 
#endif /* RR_BLOCKLOOP */
			do {
				RR_INC( cc1 );
				sum[ jp   ] = Data[ jd++   ] ^ rr_mult( rr, sum[ jp ], jm );
			} while ( ++jp < rr->blksize );
//			rr->SynData[ jp ][ jb ] = sum;
		} while ( ++jb < blk_count );
#ifdef RR_SWAP
		{
			rrcode_t *tsum = rr->syndata[ -1 ] = sum;
			rr->syndata[ jc ] = sum = sum2;
			sum2 = tsum;
		}
#endif /* RR_SWAP */
	}
	return 0;	// %%% fake
}

static 
#ifdef RR_INLINE
	__inline 
#endif /* RR_INLINE */
int gfMult( const rrtable_t *rr, int a, int b ) {
  return ( ( a==0 || b == 0 ) ? 0 :
  		rr->gfExp[ rr->gfLog[ a ] + rr->gfLog[ b ] ] );
}

int rr_restore( const rrtable_t *rr, 
//		int blk_count,	// кол-во блоков на обработку
//		int num,				// номер первого обрабатываемого блока
//		rrdata_t *Data,			// собственно данные
		int blk_total,	// сумма кол-ва блоков с данными и ремонтных 
		int secsize,		// размер блока в байтах
//		int EraLoc[], 
		int errcount,		// кол-во сбойных секторов
		int errnums[],	// их номера
		rrdata_t *errlocs[]	// их координаты 	
		) {
// %%% _очень_ медленно работает при большом размере сектора!
//  int PolD[ MAXPOL ];
//	int PolB[ MAXPOL ];
	int EraPos, Root;
//	rrbuf_t *polb = NULL, *dn = NULL, *errorlocs = NULL;
	rrbuf_t *tbuf = NULL, *pold = NULL/*, *n = NULL*/;
	int js, stream_count, jc, rc, jp;
	rrcode_t *PolB, *Dn, *ErrorLocs, *PolD, *SynData,
		*PolD2, *N;
	int *real_errpos = NULL;
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
//	FirstBlockDone = 1;
//	Clean( PolB, rr->rrcount + 1 );
	rc = -1;
	secsize /= sizeof( rrdata_t );
	stream_count = rr->blksize / secsize;
	if ( errcount > ( rr->rrcount * stream_count ) ) {
		rc = 0;
		goto exit;
	}
	if ( ( tbuf = rr_getbuf( 5, rr->rrcount + 1, 0 ) ) == NULL )
		goto exit;
	if ( ( pold = rr_getbuf( rr->rrcount + 2, secsize, 0 ) ) == NULL )
		goto exit;
	if ( ( real_errpos = malloc( sizeof( int ) * stream_count * blk_total ) )
		== NULL )
		goto exit;
//	if ( ( n = rr_getbuf( rr->rrcount, secsize, 0 ) ) == NULL )
//		goto exit;
	PolB = tbuf[ 0 ];
	Dn = tbuf[ 1 ];
	ErrorLocs = tbuf[ 2 ];
	SynData = tbuf[ 3 ];
	PolD2 = tbuf[ 4 ];
	N = pold[ rr->rrcount + 1 ];
/*
	if ( ( ( polb = rr_getbuf( stream_count, rr->rrcount + 1 ) ) == NULL ) ||
		( ( dn = rr_getbuf( stream_count, rr->rrcount + 1 ) ) == NULL ) ||
		( ( errorlocs = rr_getbuf( stream_count, rr->rrcount + 1 ) ) == NULL ) )
		goto exit;
*/
	rc = 0;
	for ( js = 0; js < stream_count; js++ ) {
/*
		rrcode_t *PolB = polb[ js ];
		rrcode_t *Dn = dn[ js ];
		rrcode_t *ErrorLocs = errorlocs[ js ];
*/
		int ErrCount = 0;
		int stream_errs = 0;
		Clean( PolB, rr->rrcount + 1 );
		Clean( Dn, rr->rrcount + 1 );
		Clean( ErrorLocs, rr->rrcount + 1 );
//		Clean( real_errpos, rr->rrcount + 1 );

		PolB[ 0 ] = 1; 
//		printf( "point 1\n" );
		for ( EraPos = 0; EraPos < ( blk_total * stream_count ); EraPos++ ) {
			real_errpos[ EraPos ] = -1;
		}
		for ( EraPos = 0; EraPos < errcount; EraPos++ ) {
			int stream_errpos;
			if ( ( errnums[ EraPos ] % stream_count ) != js )
				continue;
			stream_errpos = errnums[ EraPos ] / stream_count;
			real_errpos[ stream_errpos ] = EraPos;
			rr_gentable( jm, rr->gfExp[ blk_total - stream_errpos - 1 ] );
			for ( jc = rr->rrcount /*, 
				M = rr->gfExp[ blk_total - stream_errpos - 1 ]*/; jc > 0; jc-- )
				PolB[ jc ] ^= rr_mult( rr, /*M,*/ PolB[ jc - 1 ], jm );
			stream_errs++;
		}
//		printf( "point 2\n" );
		for ( Root = MAXPAR - blk_total; Root < MAXPAR + 1; Root++ ) {
			int Sum = 0, B;
			for ( B = 0; B < rr->rrcount + 1; B++ )
				Sum ^= gfMult( rr, rr->gfExp[ ( B * Root ) % MAXPAR ],
					PolB[ B ] );
			if ( Sum == 0 ) {
				int I;
//				int realpos;
				if ( ErrCount > rr->rrcount ) 
					goto exit;
				Dn[ ErrCount ] = 0;
				for ( I = 1; I < rr->rrcount + 1; I += 2 )
					Dn[ ErrCount ]^= gfMult( rr, PolB[I], 
					rr->gfExp[ Root * ( I - 1 ) % MAXPAR ] );
				ErrorLocs[ ErrCount++ ] = MAXPAR - Root;
			}
		}
//		printf( "point 3\n" );
		if ( ( ErrCount <= rr->rrcount ) && ErrCount > 0 ) {
			int je;
			for ( jp = 0; jp < secsize; jp++ ) {
				jc = 0; do {
					SynData[ jc ] = rr->syndata[ jc ][ js * secsize + jp ];
				} while ( ++jc < rr->rrcount );
//				if ( jp > 400 )
//					jc = jc;
				rr_pnmult( rr, PolB, SynData, PolD2 );
				jc = 0; do {
					pold[ jc ][ jp ] = PolD2[ jc ];
				} while ( ++jc < rr->rrcount );
			}
//			printf( "point 3a\n" );
			for ( je = 0; je < ErrCount; je++ ) {
				int DataPos;
				int Loc = ErrorLocs[ je ], DLoc = MAXPAR - Loc;
				Clean( N, secsize );
				for ( jc = 0; jc < rr->rrcount; jc++ ) {
					rr_gentable( jm, rr->gfExp[ DLoc * jc % MAXPAR ] );
					PolD = pold[ jc ];
					jp = 0; do /*for ( jp = 0; jp < secsize; jp++ )*/ {
//						PolD = pold[ jp ];
						N[ jp ]		// on 0..secsize, 0..ErrCount,
							^= rr_mult( rr, 
							PolD[ jp ],	// on 0..secsize, 0..rrcount
							jm
//							rr->gfExp[ DLoc * jc % MAXPAR ]	// on 0..ErrCount, 0..rrcount
						);
					} while ( ++jp < secsize );
				}
				DataPos = blk_total - Loc - 1;
				if ( DataPos >= 0 && DataPos < blk_total ) {
					int errplace = real_errpos[ DataPos ];
					if ( errplace >= 0 ) {
						rrdata_t *errpos = errlocs[	errplace ];
						rr_gentable( jm, rr->gfExp[ MAXPAR - rr->gfLog[ 
											Dn[ je ]	// on 0..ErrCount
									] ] );
						jp = 0; do /*for ( jp = 0; jp < secsize; jp++ )*/ {
							*( errpos++ ) /*^*/= rr_mult( rr,
									N[ jp++ ],	// on 0..secsize, 0..ErrCount,
									jm );
						} while ( jp < secsize );
					}
				}
			}
		}
//		printf( "point 4\n" );
/*
		for ( jp = 0; jp < secsize; jp++ ) {
			for ( jc = 0; jc < rr->rrcount; jc++ ) {
				SynData[ jc ] = rr->syndata[ jc ][ js * secsize + jp ];
			}
			rr_pnmult( rr, PolB, SynData, PolD );
			if ( ( ErrCount <= rr->rrcount ) && ErrCount > 0 ) {
//				int je;
				for ( je = 0; je < ErrCount; je++ ) {
					int DataPos;
					int J;
					int Loc = ErrorLocs[ je ], DLoc = MAXPAR - Loc, N = 0;
					for ( J = 0; J < rr->rrcount; J++ ) 
						N ^= gfMult( rr, 
							PolD[ J ],	// depends on 0..secsize, 0..rrcount
							rr->gfExp[ DLoc * J % MAXPAR ]	// on 0..ErrCount, 0..rrcount
						);
					DataPos = blk_total - Loc - 1;
					if ( DataPos >= 0 && DataPos < blk_total )
						Data[ real_errpos[ DataPos ] + jp ] ^=
							gfMult( rr, 
								N,	// on 0..secsize, 0..ErrCount,
								rr->gfExp[ MAXPAR - rr->gfLog[ 
									Dn[ je ]	// on 0..ErrCount
							] ] );
				}
			}
		}
*/
	}
	rc = 1;

//			ErrCount = 0;
exit:
/*
	if ( polb != NULL ) free( --polb );
	if ( dn != NULL ) free( --dn );
	if ( errorlocs != NULL ) free( --errorlocs );
*/
	if ( tbuf != NULL )
		free( tbuf );
	if ( pold != NULL )
		free( pold );
	if ( real_errpos != NULL )
		free( real_errpos );
//	if ( n != NULL )
//		free( n );
	return ( rc );
}
