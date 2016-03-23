/* Asp applicative language
 * J.Cupitt, November 86
 * Main section */

#include <sys/time.h>			/* Used for finding execution time. */
#include <sys/resource.h>

#include "asphdr.h"			/* Get main header */
#include "evaltypes.h"			/* Needed by symboltypes.h */
#include "lextypes.h"			/* Needed by parsetypes.h */
#include "parsetypes.h"			/* Needed by symboltypes.h */
#include "symboltypes.h"		/* Needed by symbol.h */
#include "symbol.h"			/* Get symbol handler */
#include "memory.h"			/* Get memory handler */
#include "dump.h"			/* Need dumper for debugging */
#include "subexpr.h"			/* Need subexpr remover */
#include "declare.h"			/* Need to test for UNRESOLVED */
#include "parser.h"			/* Need to see ST */
#include "compile.h"			/* Get compiler */
#include "evaluate.h"			/* Need evaluator */
#include "io.h"				/* Need to init input */
#include "error.h"			/* Error handler */

#define DASTACKSIZE 500			/* Obviously modest! */
#define DFSTACKSIZE 100			/* Fence stack */
#define DPSTACKSIZE 1000		/* Also modest */
#define DEFAULTEVAL 60000		/* Modest size for default */
#define DEFAULTPARSE 20000		/* Also modest */

/* Command line stuff. Available globally in main.h */
enum Boolean Warnings = TRUE;		/* -w not issued */
enum Boolean Extras = TRUE;		/* - not issued */
enum Boolean Counting = FALSE;		/* -c issued */
int EvalHeapSize = DEFAULTEVAL;		/* Size of eval heap */
int ParseHeapSize = DEFAULTPARSE;	/* And size of parse heap */
int AStackSize = DASTACKSIZE;		/* Application stack size */
int FStackSize = DFSTACKSIZE;		/* Fence stack */
int PStackSize = DPSTACKSIZE;		/* Pointer stack */
char *OurName;				/* Name we were called by */

/* Print a usage message & exit */
static void
usage()
{	errorstart();
	errorstr( "usage: " );
	errorstr( OurName );
	errorstr( " [-w -c - -h<n> -p<n> -a<n> -g<n> -f<n>] [{filename}]" );
	errorend();
	quit();
}

/* Get an uint from a string. Used for getting heap sizes */
static int 
GetUInt( str )
char *str;
{	int res;
	if( sscanf( str, "%u", &res) != 1 ) {
		errorstart();
		errorstr( "heap size should be unsigned int" );
		errorend();
		quit();
	}
	return( res );
}

/* Read elapsed time for this job. Return a long with the bottom
 * two digits fractions of a sec. */
static long 
dotime()
{	long r;
	struct rusage useblock;
	if( getrusage( RUSAGE_SELF, &useblock ) != 0 ) {
		errorstart();
		errorstr( "call to getrusage fails!" );
		errorend();
		quit();
	}
	r = (useblock.ru_utime.tv_usec/10000L) + 
		(useblock.ru_utime.tv_sec*100L);
	return( r );
}

/* Start here ... */
void
main( argc, argv )
int argc;
char *argv[];
{	long time;			/* Used for timing execution */
	OurName = *argv;		/* Note name we were called by */
	while( --argc ) 
		if( argv[argc][0] == '-' )
			switch( argv[argc][1] ) {
				case 'w':	
					Warnings = FALSE;
					break;
				case 'c':	
					Counting = TRUE;
					break;
				case '\0':	
					Extras = FALSE;
					break;
				case 'h':	
					EvalHeapSize = GetUInt( argv[argc]+2 );
					break;
				case 'p':	
					ParseHeapSize = GetUInt( argv[argc]+2 );
					break;
				case 'a':	
					AStackSize = GetUInt( argv[argc]+2 );
					break;
				case 'g':	
					PStackSize = GetUInt( argv[argc]+2 );
					break;
				case 'f':	
					FStackSize = GetUInt( argv[argc]+2 );
					break;
				default:	
					usage();
					break;
			}
		else 	/* It's a file name */
			AddFile( argv[ argc ] );

	InitMemory();	
	InitParseTree();
	InitEvalStacks();
	ParseFiles();
	TestDeclared();
	CommonSub();

#ifdef DEBUG
	printf( "Dumping ST:\n" );
	DumpST( CurrentTable );
#endif

	CompileTable();
	if( Counting )
		printf( "# Generated %d nodes\n", UsedSpace() );
	if( Extras )
		printf( "Starting evaluation\n" );
	time = dotime();	/* Read clock */
	EvaluateScript();
	time = dotime() - time;	/* Elasped CPU */
	if( Extras )
		printf( "Finished\n" );

#ifdef TREES
	printf( "Graph after reduction is:\n" );
	DumpGraph( 1, &ScriptRoot->details.func.code );
#endif

	if( Counting ) {
		printf( "# %d compactions, %d reductions\n", 
			NumberCompacts, NumberReductions );
		printf( "# Graph after reduction contains %d nodes\n", 
			CountGraph( &ScriptRoot->details.func.code ) );
		printf( "# %d nodes in use\n", UsedSpace() );
		printf( "# %.2f secs CPU\n", (float) time/100L );
	}
}
