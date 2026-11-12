#ifndef __BUFFER_H__
#define __BUFFER_H__


/*
Usage:

   BufInit: initializes buffer, do always before using it, needs a valid filedescriptor
   BufWriteSeek: seeks to a certain point and flush buffer if needed, always 
    recommended before writing if you plan to do non-sequential writes
   BufRead/BufWrite: read or write data from/to the buffered file
   BufWriteFlush: flush buffered data to the file being written, needed after closing it
   BufFree: free buffer

*/

struct buffer_s {
       FILE *file;
       char *ptr;
       long index;
       long size;
       };

int BufRead (char *data, long size, struct buffer_s *buffer, long filesize);
int BufWrite (char *data, long size, struct buffer_s *buffer);
int BufWriteFlush (struct buffer_s *buffer);
int BufWriteSeek (long seekpoint, struct buffer_s *buffer);
int BufInit (FILE *file, long bufsize, struct buffer_s *buffer);
int BufFree (struct buffer_s *buffer);

#define READ_BUF_SIZE  512*1024
#define WRITE_BUF_SIZE 1024*1024

#endif

