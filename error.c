/* Asp applicative language
 * J.Cupitt, November 86
 * Error handler */

#include "asphdr.h"			/* Get standard header */
#include "io.h"				/* Need lineno, filename */
#include "main.h"			/* Need our name */

/* Start an error message. Just print the standard 'foo: '
 * Exported */
void errorstart()
{	fprintf( stderr, "%s: ", OurName );
};

/* Print an error message, plus a line number.
 * Exported. */
void errorln( str )
char *str;
{	fprintf( stderr, "at %d:%s: %s", lineno, filename, str );
};

/* Print an error message.
 * Exported. */
void errorstr( str )
char *str;
{	fprintf( stderr, "%s", str );
};

/* Print an error message.
 * Exported. */
void errornum( n )
int n;
{	fprintf( stderr, "%d", n );
};

/* End error message.
 * Exported. */
void errorend()
{	fprintf( stderr, "\n" );
};

/* Leave with an error exit.
 * Exported. */
void quit()
{	exit(1);
};

/* Export for yacc */
void yyerror( str )
char *str;
{	errorstart();
	errorln( str );
	errorend();
};


