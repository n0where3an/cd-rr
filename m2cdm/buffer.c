#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "buffer.h"


int BufInit (FILE *file, long bufsize, struct buffer_s *buffer)
{
     buffer->file = file;
     buffer->size = bufsize;
     buffer->index = 0;
     buffer->ptr = (char *) malloc(bufsize);

     if (buffer->ptr == NULL)
        return 0; // error

return 1;
}

int BufFree (struct buffer_s *buffer)
{
    free(buffer->ptr);

return 1;
}

int BufWrite (char *data, long data_size, struct buffer_s *buffer)
{
     long write_length;
     int ret = 1;

     if (data_size > (buffer->size + (buffer->size - buffer->index - 1)))
        return 0;  // unimplemented

     if (buffer->index + data_size < buffer->size)  // 1 menos
     {
        memcpy ((buffer->ptr + buffer->index), data, data_size);
        buffer->index += data_size;
     }
     else
     {
         write_length = buffer->size - buffer->index;
         memcpy ((buffer->ptr + buffer->index), data, write_length);
         if (!fwrite (buffer->ptr, buffer->size, 1, buffer->file))
            ret = 0; // error
         memcpy (buffer->ptr, data + write_length, data_size - write_length);
         buffer->index = data_size - write_length;
     }

return ret;
}

int BufWriteFlush (struct buffer_s *buffer)
{
     int ret = 1;
    
     if (buffer->index == 0)
        return ret; // nothing to write

     if (!fwrite (buffer->ptr, buffer->index, 1, buffer->file))
        ret = 0; // error
     buffer->index = 0;

return ret;
}

int BufWriteSeek (long seekpoint, struct buffer_s *buffer)
{
     int ret = 1;

     if (seekpoint == ftell(buffer->file) + buffer->index)
        return ret; // no need to seek
     else
     {
         BufWriteFlush(buffer); // flush buffer before seeking
         fseek (buffer->file, seekpoint, SEEK_SET);
     }

return ret;
}

int BufRead (char *data, long data_size, struct buffer_s *buffer, long filesize)
{
     long read_length, max_length, pos;

     if (data_size > (buffer->size + (buffer->size - buffer->index - 1)))
        return 0;  // unimplemented

     if (filesize == 0)  // no cuenta
     {
        max_length = buffer->size;
     }
     else
     {
        pos = ftell(buffer->file);
        if (pos > filesize) max_length = 0;
        else max_length = ((pos + buffer->size) > filesize) ? (filesize - pos) : buffer->size;
     }

     if (buffer->index == 0)
     {
        fread(buffer->ptr, max_length, 1, buffer->file);
     }

     if (buffer->index + data_size <= buffer->size)
     {
        memcpy (data, buffer->ptr + buffer->index, data_size);
        buffer->index += data_size;
        if (buffer->index >= buffer->size) buffer->index = 0;
     }
     else
     {
         read_length = buffer->size - buffer->index;
         memcpy (data, buffer->ptr + buffer->index, read_length);
         fread (buffer->ptr, max_length, 1, buffer->file);
         memcpy (data + read_length, buffer->ptr, data_size - read_length);
         buffer->index = data_size - read_length;
     }

return 1;
}


