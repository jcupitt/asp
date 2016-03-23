/* Asp applicative language
 * J. Cupitt, November 86
 * Post-parse pass of symbol table. We:
 *	- Test all symbols have been declared
 * 	- Find and mark all non-lambda enclosed functions
 * 	- Fix up extern lists, removing PARAMs and TOPFUNCs */

#include "asphdr.h"			/* Get global header */
#include "evaltypes.h"			/* Needed by symboltypes */
#include "parsetypes.h"			/* Needed by symboltypes */
#include "symboltypes.h"		/* Get types we manipulate */
#include "symbol.h"			/* Need to lookup start function */
#include "error.h"			/* Need error handling */
#include "parser.h"			/* Need global table */
#include "list.h"			/* Need hd, tl etc. */

#define STARTFN "main"			/* Name of start function */
struct Symbol *ScriptRoot;		/* Root expression for program */

/* Mark all outermost lambdas. This function is mapped onto the global
 * symbol table */
static void MarkLambdas( sym )
struct Symbol *sym;
{	/* May occasionally see a PARAMETER .. see below */
	if( !ISFUNC(sym) )
		return;
	if( sym->details.func.nargs != 0 )
		/* Has some args .. therefore we have an outermost
		 * lambda */
		sym->tag = TOPFUNC;
	else { 	/* No args .. any local defs will therefore be top
		 * lambdas. */
		sym->tag = TOPFUNC;
		ForAllSym( sym->details.func.locals, MarkLambdas );
	};
};

/* Scan an extern list, dropping silly externs */
static struct SymbolList *DropExterns( slist )
struct SymbolList *slist;
{	struct Symbol *sym;
	if( slist == NULL )
		return( NULL );
	sym = hd(slist);
	if( ISPARM(sym) || sym->tag == TOPFUNC )
		/* Not an extern if:
		 * 	- A parameter
		 *	- A top-level definition */
		return( DropExterns( tl(slist) ) );
	/* An OK extern. Keep it & pass on. */
	slist->next = DropExterns( tl(slist) );
	return( slist );
};	

/* Look at all those externs and remove any references to params or TOPFUNCS
 * or ourselves. */
static void FixExtents( sym )
struct Symbol *sym;
{	if( ISFUNC(sym) ) {
		/* Do locals first */
		ForAllSym( sym->details.func.locals, FixExtents );
		/* And scan extern list, dropping inappropriate externs */
		sym->details.func.externs = 
			DropExterns( sym->details.func.externs );
	}
};

static enum Boolean errorflg;		/* Just a flag .. see below */

/* Test a symbol for declaredness */
static void TestSymbol( sym )
struct Symbol *sym;
{	if( sym->tag == UNRESOLVED ) {
		errorstart();
		errorstr( "function " );
		errorstr( sym->sname );
		errorstr( " used at " );
		errornum( sym->declaredat );
		errorstr( ":" );
		errorstr( sym->fileat );
		errorstr( " but never declared" );
		errorend();
		errorflg = TRUE;
	};
};

/* Test for all symbols declared.
 * Exported. */
void TestDeclared()
{	/* Scan top level ST finding UNRESOLVED names. If any, quit
 	 * compile. */
	errorflg = FALSE;
	ForAllSym( CurrentTable, TestSymbol );
	if( errorflg ) 
		quit();
	/* Find start function & do simple checks */
	ScriptRoot = FindName( CurrentTable, STARTFN );
	if( !ISFUNC(ScriptRoot) ) {
		errorstart();
		errorstr( "start function not defined" );
		errorend();
		quit();
	};
	if( ScriptRoot->details.func.nargs != 0 ) {
		errorstart();
		errorstr( "start function " );
		errornum( ScriptRoot->declaredat );
		errorstr( ":" );
		errorstr( ScriptRoot->fileat );
		errorstr( " should have no arguments" );
		errorend();
		quit();
	};
	ScriptRoot->used = TRUE;	/* As we see it! */
	/* Mark all functions not inside lambdas */
	ForAllSym( CurrentTable, MarkLambdas );
	/* Scan all those extern links, removing params and topfuncs */
	ForAllSym( CurrentTable, FixExtents );
};
