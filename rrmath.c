#include "rrm_int.h"
#ifdef RRDEBUG
__int64 cc1 = 0, cc2 = 0;
#endif /* RRDEBUG */

#ifdef RRDEBUG
	void printtbl( char *title, rrcode_t *tbl, int size ) {
		int jx, jy = 0;
		printf( "\n=== %s ===\n", title );
		for ( jx = 0; jx < size; jx++ ) {
			if ( tbl[ jx ] < 0 ) 
				printf( EMPTY " " );
			else {
				printf( HEXOUT " ", tbl[ jx ] );
				jy++;
			}
			if ( ( ( jx + 1 ) % 16 ) == 0 ) printf( "\n" );
		}
		printf( "+++ totals: %d +++\n", jy );
	}
#endif /* RRDEBUG */

//int poly[ 256 ];
#ifdef RR_NO_TABLE
	void rr_gentable_( rrcode_t *jp, rrcode_t b ) {
		int jx, jy;
		for ( jx = 0; jx < 8; jx++ ) {
			register rrcode_t part = _rotl( b, jx << 2 );
			register rrcode_t joff = INDEX_OFFSET( jx );
			jp[ joff++ ] = 0; jp[ joff++ ] = part;
			for ( jy = 1; jy < ( 1 << ( LINE_OFFSET >> 1 ) ); jy++ ) {
				register rrcode_t part2 = _rotl( jp[ joff - jy ], 1 );
				jp[ joff++ ] = part2;
				jp[ joff++ ] = part2 ^ part;
			}
		}
	}
#else /* RR_NO_TABLE */
	void rr_gentable_( unsigned int *table, unsigned int b ) {
		unsigned int jx, jy;
		if ( b != table[ 1 ] ) {
			table[ 0 ] = 0; table[ 1 ] = b;
			jx = 4; do /*for ( jx = 4; jx <= RR_MAXHALF; jx <<= 1 )*/ {
				jy = jx >> 1; do /*for ( jy = jx >> 1; jy < jx; jy += 2 )*/ {
					table[ jy ] = table[ jy >> 1 ] << 1;
					table[ jy + 1 ] = table[ jy ] ^ b;
					jy += 2;
				} while ( jy < jx );
				jx <<= 1;
			} while ( jx <= RR_MAXHALF );
		}
	}
#endif /* RR_NO_TABLE */
void rr_pnmult( const rrtable_t *rr, rrcode_t *p1, rrcode_t *p2, rrcode_t *r ) {
	int jx;
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
	Clean( r, rr->rrcount );
//      jx = _rotl( rr->rrcount, rr->blksize );
	jx = 0; 
	do {
		int jz = rr->rrcount - jx;
		rrcode_t jp = p1[ jx ];
		if ( jp != 0 ) {
			int jy;
			rr_gentable( jm, jp );
#ifdef RR_FORLOOP
			for ( jy = 0; jy < jz; jy++ ) {
				r[ jx + jy ] ^= rr_mult( rr, p2[ jy ], jm );
			}
#else /* RR_FORLOOP */
			jy = 0; 
#ifdef RR_BLOCKLOOP
			if ( jy - jz <= -4 ) do {
				int jt = jx + jy;
				r[ jt     ] ^= rr_mult( rr, p2[ jy     ], jm );
				r[ jt + 1 ] ^= rr_mult( rr, p2[ jy + 1 ], jm );
				r[ jt + 2 ] ^= rr_mult( rr, p2[ jy + 2 ], jm );
				r[ jt + 3 ] ^= rr_mult( rr, p2[ jy + 3 ], jm );
				jy += 4;
			} while ( jy - jz <= -4 );
			if ( jy < jz )
#endif /* RR_BLOCKLOOP */
			do {
				r[ jx + jy ] ^= rr_mult( rr, p2[ jy ], jm );
			} while ( ++jy < jz );
#endif /* RR_FORLOOP */
		}
	} while ( ++jx < rr->rrcount );
}
int rr_getcount( const rrtable_t *rr ) {
	return ( rr->rrcount );
}
rrtable_t *rr_init( /* int RR_BASELEN, int magic, */
		int rrcount, int secsize ) {
	int 
		magic = RR_MAGIC,
		maxbyte = 1 << RR_BASELEN,
		maxpar = maxbyte - 1,
		maxpol = 2 * maxbyte;
	rrtable_t *rr;
//      rrbuf_t *rrbuf; // *SynData;
	int jx, jy;
	int 
		size1 = sizeof( rrtable_t ),    // а куда денешься - в стек?
		size2 = sizeof( rrcode_t ) * rrcount;   // для pnInit
//              rrbufsize = ( rrcount + 1 )/*secsize*/ * sizeof( rrbuf_t )      // rrdata
//                      + secsize * sizeof( rrbuf_t )   // GXPol
//                      + secsize * ( rrcount + 1 ) * sizeof( rrcode_t );
	rrcode_t *p1, *p2;

//              fullsize = size1 + ( ( size2 > rrbufsize ) ? size2 : rrbufsize );
		// для области данных rrbuf_t

	if ( /* ( baselen > RR_BASELEN ) || */ ( ( rr = malloc( size1 ) ) == NULL ) ) 
		goto err1;
	memset( rr, 0, size1 );
#ifdef RR_BIGTABLE
	for ( jx = 0; jx < RR_MAXBYTE; jx++ ) {
		rr_gentable_( &rr->multtbl[ jx ][ 0 ], jx );
	}
#endif /* RR_BIGTABLE */
//#pragma prefetch rr->poly
	rr_gentable( rr->poly, magic );
	magic = ( magic & maxpar ) | maxbyte;
	rr->baselen = RR_BASELEN; rr->magic = magic; rr->rrcount = rrcount;
	rr->blksize = secsize / sizeof( rrdata_t );
//      rr->gfExp = rr->full_exp + MAXPOL;
// gfInit:
	jx = 0; jy = 1/*(magic & maxpar)*/; do {
		rr->gfExp[ jx ] = (rrcode_t) jy;
		rr->gfLog[ jy ] = (rrcode_t) jx;
		if ( ( jy <<= 1 ) & maxbyte ) {
			jy ^= magic;
    }
	} while ( ++jx < maxpar );
	jy = 0; do {
		rr->gfExp[ jx++ ] = rr->gfExp[ jy++ ];
	} while ( jx < maxpol );
//      memcpy( rr->gfExp + maxpar, rr->gfExp, sizeof( int ) * ( maxpol - maxpar ) );
	printtbl( "gfLog", rr->gfLog, maxbyte );
	printtbl( "gfExp", rr->gfExp, maxpol );
// pnInit:
	if ( ( p1 = malloc( 2 * size2 ) ) == NULL )
		goto err2;
	p2 = p1 + rrcount;
	Clean( p2, rrcount );
	memset( p2, 0, size2 );
  
	p2[ 0 ] = 1;
	jx = 1; do {
    Clean( p1, rrcount );
    p1[ 0 ] = rr->gfExp[ jx ];
    p1[ 1 ] = 1;
    rr_pnmult( rr, p1, p2, rr->GXPol );
		jy = 0; do {
      p2[ jy ] = rr->GXPol[ jy ];
    } while ( ++jy < rrcount );
	} while ( ++jx <= rrcount );
	printtbl( "GXPol", rr->GXPol, /*maxpol * 2*/ maxbyte );
	free( p1 );

	rr->rrdata = NULL;
	rr->syndata = NULL;

#ifdef RRDEBUG
	for ( jx = 0; jx < rrcount; jx++ ) {
		int jy;
		if ( ( jy = rr->gfExp[ rr->gfLog[ jx ] ] ) != jx )
			printf( "gfExp[ gfLog[ %d ] ] = %d != %d\n", jx, jy, jx );
		if ( ( jy = rr->gfLog[ rr->gfExp[ jx ] ] ) != jx )
			printf( "gfLog[ gfExp[ %d ] ] = %d != %d\n", jx, jy, jx );
	}
	{
		int jx, jy;
		printf( "=== MultTable ===\n" );
		printf( "    :" );
		for ( jy = 0; jy <= 255; jy++ ) {
			printf( " %3d", jy );
			if ( ( jy & 15 ) == 15 )
				printf( " :" );
		}
		printf( "\n" );
		printf( "- - +" );
		for ( jy = 0; jy <= 255; jy++ ) {
			printf( " - -" );
			if ( ( jy & 15 ) == 15 )
				printf( " +" );
		}
		printf( " - -\n" );
		for ( jx = 0; jx <= 255; jx++ ) {
			printf( "%3d :", jx );
			for ( jy = 0; jy <= 255; jy++ ) {
				int ff = /*jx * jy - */gfMult( rr, jx, jy );
				printf( " %3d", ff );
				if ( ( jy & 15 ) == 15 )
					printf( " :" );
			}
			printf( " %3d\n", jx );
			if ( ( jx & 15 ) == 15 ) {
				printf( "- - +" );
				for ( jy = 0; jy <= 255; jy++ ) {
					printf( " - -" );
					if ( ( jy & 15 ) == 15 )
						printf( " +" );
				}
				printf( " - -\n" );
			}
		}
		printf( "    :" );
		for ( jy = 0; jy <= 255; jy++ ) {
			printf( " %3d", jy );
			if ( ( jy & 15 ) == 15 )
				printf( " :" );
		}
		printf( "\n" );
	}
#endif /* RRDEBUG */
	return ( rr );
err2:
	free( rr );
err1:
	return NULL;
}

rrbuf_t *rr_getbuf( int height, int width, int extra ) {
	int jx;
	int bufsize;
	rrbuf_t *rrbuf;
	rrcode_t *p2;

	height += extra;
	bufsize = height * ( width * sizeof( rrcode_t ) + sizeof( rrbuf_t ) );
	if ( ( rrbuf = malloc( bufsize ) ) == NULL )
		goto err2;
	memset( rrbuf, 0, bufsize );
// rrbuf_t:
	p2 = (rrcode_t *) ( rrbuf + height );
	for ( jx = 0; jx < height; jx++ ) {
		rrbuf[ jx ] = p2; p2 += width;
	}
	rrbuf += extra;
err2:
	return rrbuf;
}
void rr_cleanup( rrtable_t *rr ) {
	if ( rr->rrdata != NULL ) 
		free( rr->rrdata - RR_EXTRALINE );
	if ( rr->syndata != NULL ) 
		free( rr->syndata - RR_EXTRALINE );
	free( rr );
}

