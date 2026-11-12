/*

    2012/11/02 - 1.5.1e - Elicit Gingers

    raw96.h: function prototype added to avoid stupid warning

*/

#ifndef I_RAW96
#define I_RAW96

#define BLOCKS2MIN(x) ((x)/(75*60))
#define BLOCKS2SEC(x) (((x)/75)%60)
#define BLOCKS2FRA(x) ((x)%75)

#define LBA2MIN(x) (BLOCKS2MIN(x+150))
#define LBA2SEC(x) (BLOCKS2SEC(x+150))
#define LBA2FRA(x) (BLOCKS2FRA(x+150))

#define MSF2LBA(m,s,f) (((((m)*60*75)+(s)*75)+(f))-150)

#pragma pack (push, 1)

typedef struct
{
	char TNO, INDEX, MIN, SEC, FRAME, ZERO, AMIN, ASEC, AFRAME;
} SUB_Q1;

typedef struct
{
	char NZ[7], ZERO, AFRAME;
} SUB_Q2;

typedef struct
{
	char data[9];
} SUB_Q3;

typedef struct
{
	char control_addr;
	union
	{
		SUB_Q1 q1_data;
		SUB_Q2 q2_data;
		SUB_Q3 q3_data;
	};
	char crc[2];
} SUB_Q;

typedef struct 
{
	char  p[12];
	SUB_Q q;
	char  r[12],s[12],t[12],u[12],v[12],w[12];
} SUBBLOCK;

#pragma pack (pop)

int	do_encode_sub_pw(SUBBLOCK* lpsbDest, int tno, int index, int rec_rel, int rec_abs);

int bin2bcd( int x );

#endif /* I_RAW96 */

