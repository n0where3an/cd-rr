// Часть библиотеки RRFILE (входит в проект "cd-R eRRoR coRRectoR")
// 
// Адаптирован для библиотеки работы с XCD-файлами FLX 0.13
// (c) Рыжики Пикчерз
// специально для Mike Matsnev aka Haali
// для связывания с MatroskaSplitter'ом

// todo: реализация спецификации FLX в полном объёме

#ifndef __XCDHDRS_H__
#define __XCDHDRS_H__

#pragma pack( 1 )
struct riffhdr_s {
	char riff_tag[ 4 ];					// "RIFF"
	unsigned int rifflen;				// file length - 8
	char type_tag[ 4 ];					// "CDXA"
	char fmt_tag[ 4 ];					// "fmt "
	unsigned int fmtlen;				// 0x10
	unsigned short format;			// 0
	unsigned short channels;		// 0
	unsigned int samples;				// per sec: 0x41585515 - "$UXA"
	unsigned int bytes;					// per sec: 1
	unsigned short align;				// 0
	unsigned short samplesize;	// 0
	char data_tag[ 4 ];					// "data"
	unsigned int datalen;				// file length - 0x2C
};
#define RIFF_LEN sizeof( struct riffhdr_s )
#define XCD_SYNC_LEN 12
#define XCD_HDR_LEN 4
#define XCD_SUBHDR_LEN 8
#define XCD_DATA_LEN 2324
struct rrblock_s {
	unsigned char sync[ XCD_SYNC_LEN ];
	unsigned char header[ XCD_HDR_LEN ];
	unsigned char subheader[ XCD_SUBHDR_LEN ];
	unsigned char data[ XCD_DATA_LEN ];
	unsigned int edc;
};
#pragma pack()
#define XCD_LEN sizeof( struct rrblock_s )

#endif /* __XCDHDRS_H__ */
