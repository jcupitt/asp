/* Asp applicative language
 * J.Cupitt, November 86
 * io handling for lex. */

#include "asphdr.h"			/* Get main header */
#include "error.h"			/* Need some error handling */

#define MAXFILES 50			/* The max number of files on
					 * command line */

char *files[ MAXFILES ];		/* Files we have to compile */
int nfiles = 0;				/* Number of files */
FILE *InputStream;			/* File we are reading from */
int lineno;				/* Current line number */
char *filename;				/* Name of file being parsed */

/* Read a character from the input stream. Count newlines.	
 * Exported */
int input()
{	int ch;
	ch = fgetc( InputStream );
	if( ch == '\n' )
		++lineno;
	if( ch == EOF )
		return( 0 );
	else
		return( ch );
};

/* Push a character back into the input stream.
 * Exported */
void unput( ch )
int ch;
{	ungetc( ch, InputStream );
	if( ch == '\n' ) 
		--lineno;
};

/* Output a character.
 * Should not be called by lex!
 * Exported */
int output( ch )
int ch;
{	errorstart();
	errorstr( "lex called output!!" );
	errorend();
	quit();
};
	
/* Open with care */
static FILE *OpenIt( name )
char *name;
{	FILE *fd;
	fd = fopen( filename, "r" );
	if( fd == NULL ) {
		errorstart();
		errorstr( "unable to open " );
		errorstr( name );
		errorstr( " for input" );
		errorend();
		quit();
	};
	return( fd );
};

/* Start up the input stream for a new file. */
static void InitInput( file ) 
FILE *file;
{	InputStream = file;
	lineno = 1;
};

/* Wrap up input. Go onto the next file, if any pending.
 * Exported to lex. 
 */
int yywrap()
{	FILE *fd;
	fclose( InputStream );
	if( !nfiles )
		/* None left .. give up */
		return( 1 );
	/* Start on next file */
	filename = files[ --nfiles ];
	fd = OpenIt( filename );
	InitInput( fd );
	/* Carry on with parse */
	return( 0 );
};

/* Parse a list of files. If none there, do stdin. 
 */
void ParseFiles()
{	if( !nfiles ) {			/* If no files named in command line */
		filename = "stdin";	/* Name this file! */
		InitInput( stdin );
	}
	else {	FILE *fd;
		filename = files[ --nfiles ];
		fd = OpenIt( filename );
		InitInput( fd );
	};
	yyparse();			/* Parse files */
};

/* Add a file to list to be parsed. 
 */
void AddFile( name )
char *name;
{	if( nfiles >= MAXFILES ) {
		errorstart();
		errorstr( "too many pending INCLUDEs" );
		errorend();
		quit();
	};
	files[ nfiles++ ] = name;
};
	
