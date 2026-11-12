/*

    2012/11/02 - 1.5.1e - Elicit Gingers

    funcs.h: some function prototypes added to avoid stupid warning

*/
 
#ifndef __funcs_h___
#define __funcs_h___

void to_dchar( char *dest, char *src, int max );
int mk_vcd_iso_fs( entry_s **entry, int num_entries, dir_s **dir_entry,
		int num_dirs, opts_s *opts, int pass );
void output_toc_file( FILE *fd_toc, entry_s **entry, int num_entries, int num_tracks,
		int last_extent, char *binfilename );
void output_cue_file( FILE *fd_cue, entry_s **entry, int num_entries, int num_tracks,
		char *binfilename );
void output_ccd_file( FILE *fd_ccd, entry_s **entry, int num_entries, int num_tracks,
		int last_extent );
void output_form1( int rec, unsigned char *data );
#endif // __funcs_h___
