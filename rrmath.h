/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef __RRMATH_H__
#define __RRMATH_H__

//typedef unsigned char rrdata_t;
#ifndef __GCC__
typedef unsigned __int16 rrdata_t;
#else
    #include <stdint.h>
    typedef uint16_t rrdata_t;
#endif

struct rrtable_s;
typedef struct rrtable_s rrtable_t;

rrtable_t *rr_init(	// инициализация. return NULL, если не удалось
//		int baselen,	// размер элемента - 1 или 2 байта
//		int magic,		// magic number для генерации таблиц
		int rrcount,	// кол-во кластеров RR-данных
		int size );		// размер кластера = размер сектора * секторов/кластер

void rr_cleanup( rrtable_t *rr );	// чистка/удаление/free

int rr_encode(	// кодирование. return -1 если что-то не удалось, иначе blk_count
		/*const*/ rrtable_t *rr,	// инициализированные таблицы
		int blk_count,	// кол-во кластеров для обсчёта
		rrdata_t *data	// обсчитываемая область (длиной blk_count * rr_init::size)
		/*, int DataSize, int *DestData */ );

int // -1 - залёт, иначе кол-во посчитанных блоков
		// (было: "== 0 - OK, == 1 - можно сбрасывать сектор переполнения")
rr_dump(	// "сброс" ремонтных данных в ремонтный сектор
		/*const*/ rrtable_t *rr, 
		int blk_count, // кол-во секторов
		int num,		// номер первого ремонтного сектора
		rrdata_t *data	// собственно данные
		);
int rr_decode( /*const*/ rrtable_t *rr, 
		int blk_count,	// кол-во блоков на обработку
//		int num,				// номер первого обрабатываемого блока
		rrdata_t *Data			// собственно данные
//		int blk_total,	// сумма кол-ва блоков с данными и ремонтных 
//		int *EraLoc, int EraSize 
		);
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
		);

//int rr_getcount( const rrtable_t *rr );


#endif /* __RRMATH_H__ */
