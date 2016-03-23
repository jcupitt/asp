/* Asp applicative language
 * J. Cupitt, November 86
 * Symbol table handler */

#include "asphdr.h"			/* Get global header */
#include "lextypes.h"			/* Needed by symboltypes */
#include "evaltypes.h"			/* Needed by symboltypes */
#include "parsetypes.h"			/* Needed by symboltypes */
#include "symboltypes.h"		/* Get types we manipulate */
#include "error.h"			/* Need error handling */
#include "io.h"				/* Need line numbers */
#include "main.h"			/* Need current filename */
#include "memory.h"			/* Need memory allocation */
#include "list.h"			/* Get list handling */
#include "dump.h"			/* Get prettyprinter */

#define SSTACKSIZE 100			/* Size of symbol stack */

/* Stack of symbols .. see parser */
static struct Symbol *SymbolStack[ SSTACKSIZE ];
static int ssp = 0;

/* The main symbol table is exported from the parser in parser.h. It
 * contains the names of all the top level definitions. Each symbol
 * in this table uses it's details.func.locals field to hold another
 * SymbolTable containing all of it's parameters and the Symbol structs
 * of any local functions. Similarly for those Symbols structs ..
 * See parser.y for how this structure is built & the use made of
 * PushTable, PopTable, ResolveNames (from the bottom of this file). */

/* A SymbolTable is defined in symboltypes.h.
 * Each entry in a symbol table is a (possibly empty) list of symbols
 * whose name hashes to that index in the table. To look up a name we hash
 * it, then do a linear search down the name list.
 * This group of functions implements symbol table handling */

/* Generate a hash key - not very sophisticated */
static int hash( table, name )
struct SymbolTable *table;
register char *name;
{	register int n = 1;
	while( *name++ != '\0' )
		n += *name;
	return( (n & 0xffff) % table->hsize );
};

/* Look along a SymbolList for a named symbol.
 * Return NULL, or a pointer to the required symbol. */
static struct Symbol *LookList( list, name )
char *name;
struct SymbolList *list;
{	if( list == NULL )
		return( NULL );
	else 
		if( strcmp( name, hd(list)->sname ) == 0 ) 
			return( hd(list) );
		else
			return( LookList( tl(list), name ) );
};

/* Unlink a symbol from a list. Return the list with that element
 * unlinked. */
static struct SymbolList *UnlinkSym( list, sym )
struct SymbolList *list;
struct Symbol *sym;
{	if( list == NULL )
		return( NULL );
	if( hd(list) == sym )
		/* Unlink this one! */
		return( tl(list) );
	else {
		/* Return this node consed to the rest of the list */
		list->next = UnlinkSym( tl(list), sym );
		return( list );
	};
};

/* Look up a name in a symbol table. Return NULL for not there, or
 * a pointer to the symbol. */
static struct Symbol *LookTable( table, name )
struct SymbolTable *table;
char *name;
{	struct SymbolList *list;
	list = (table->base)[ hash( table, name ) ];
	return( LookList( list, name ) );
};

/* Make a new table. Pass the size of the hash vector reqd.
 * Exported */
struct SymbolTable *CreateTable( n )
int n;
{	struct SymbolTable *t;
	int i;
	t = GetVec(struct SymbolTable);
	t->hsize = n;
	t->base = (struct SymbolList **) 
		allocate( n * sizeof(struct ExpressionList *) );
	for( i = 0; i < n; i++ )
		(t->base)[i] = NULL;
	return( t );
};

/* Add a symbol to a table. */
static void AddSym( table, sym )
struct SymbolTable *table;
struct Symbol *sym;
{	int n;
	n = hash( table, sym->sname );
	(table->base)[n] = scons(sym,(table->base)[n]);
};

/* Delete a sym from a table .. used by ResolveRefs */
static void DeleteSym( table, sym )
struct SymbolTable *table;
struct Symbol *sym;
{	int n;
	n = hash( table, sym->sname );
	(table->base)[n] = UnlinkSym( (table->base)[n], sym );
};

/* Apply a void function to every symbol in a table. Helps to hide the
 * symbol table structure from other modules that may need to scan STs.
 * Exported */
void ForAllSym( table, fn )
struct SymbolTable *table;
void (*fn)();
{	register int i;
	i = table->hsize;
	while( i-- ) {
		struct SymbolList *t;
		t = table->base[i];
		while( t != NULL ) {
			fn( hd(t) );
			t = tl(t);
		};
	};
};

/* This next set of functions are called from the parser to define
 * and reference symbols. */

/* Make the root symbol. The global ST hangs off this. */
struct Symbol *MakeRoot()
{	struct Symbol *t;
	t = GetVec(struct Symbol);
	t->sname = "$$ROOT";
	t->tag = FUNCTION;
	t->declaredat = -1;			/* Silly! */
	t->fileat = "$$NoFile";
	t->used = TRUE;
	t->refedat = NULL;
	t->erefedat = NULL;
	t->details.func.nargs = 0;
	t->details.func.params = NULL;
	t->details.func.externs = NULL;
	return( t );
};
	
/* Add a name to a table. This is called for a defining occurence of an
 * identifier, so we flag a nameclash if it already exists. If this name
 * has been previusly referenced in this scope, we find an UNRESOLVED
 * symbol. Can then turn this into a FUNCTION symbol. The parser uses
 * AddBody etc. later on to fill in the rest of the fields in the Symbol.
 * Note: This routine is used both for defining function names and for
 * adding parameter names .. AddParam changes sym->tag to PARAMETER!
 * Exported */
struct Symbol *AddName( table, name )
struct SymbolTable *table;
char *name;
{	struct Symbol *t;
	t = LookTable( table, name );
	if( t == NULL ) {
		/* Not there .. make a new symbol & stick it in */
		t = GetVec(struct Symbol);
		t->sname = name;
		t->tag = FUNCTION;	/* May not be .. */
		t->declaredat = lineno;
		t->fileat = filename;
		t->used = FALSE;
		t->refedat = NULL;
		t->erefedat = NULL;
		t->details.func.nargs = 0;
		t->details.func.params = NULL;
		t->details.func.externs = NULL;
		AddSym( table, t );
		return( t );
	}
	else if( t->tag == UNRESOLVED ) {
		/* Already there, but only referenced. */
		t->tag = FUNCTION;
		t->declaredat = lineno;
		t->fileat = filename;
		t->details.func.nargs = 0;
		t->details.func.params = NULL;
		t->details.func.externs = NULL;
		return( t );
	}
	else {
		/* Already there .. it's an error. */
		errorstart();
		errorln( "declaration clashes with declaration of " );
		errorstr( t->sname );
		errorstr( " " );
		errornum( t->declaredat );
		errorstr( ":" );
		errorstr( t->fileat );
		errorend();
		quit();
	};
};

/* Lookup a name in a table. This is called by the parser for binding
 * occurences of names (any use within a function body). If the name is
 * not there, this is a forward reference .. we make an UNRESOLVED
 * symbol and return that. When that name is later defined, the UNRESOLVED
 * is changed to FUNCTION or PARAMETER. Before compiling, a quick check is
 * made to look for any symbols which are still UNRESOLVED.
 * Exported */
struct Symbol *FindName( table, name )
struct SymbolTable *table;
char *name;
{	struct Symbol *t;
	t = LookTable( table, name );
	if( t == NULL ) {
		/* Not there. Make a symbol */
		t = GetVec(struct Symbol);
		t->sname = name;
		t->tag = UNRESOLVED;
		t->declaredat = lineno;
		t->fileat = filename;
		t->used = TRUE;
		t->refedat = NULL;
		t->erefedat = NULL;
		AddSym( table, t );
		return( t );
	}
	else {	/* It's been referenced .. make sure we have flagged it as
		 * used */
		t->used = TRUE;
		return( t );
	};
};	

/* Add a body to a function.
 * This function is exported. */
void AddBody( symbol, body )
struct Symbol *symbol;
struct ExpressionNode *body;
{	symbol->details.func.body = body;
};

/* Add a table to a function. Symbol tables hanging off functions hold the
 * parameter names and the names of any local functions.
 * Exported */
void AddTable( symbol, table )
struct Symbol *symbol;
struct SymbolTable *table;
{	symbol->details.func.locals = table;
};

/* Add a reference to a symbol. UNRESOLVED symbols have a list of pointers
 * to the nodes in the parse tree in which they are referenced. In some cases
 * it is necessary to patch these pointers to point to the real symbol ..
 * see ResolveNames below 
 * Exported */
void AddReference( sym, n )
struct Symbol *sym;
struct ExpressionNode *n;
{	if( sym->tag == UNRESOLVED )
		sym->refedat = 
			pcons( (int *) &n->details.ident, sym->refedat);
};

/* Add a parameter to a symbol. Uses AddName, and zaps the tag field. Also
 * sticks the param onto the param list.
 * Exported */
void AddParam( sym, name )
struct Symbol *sym;
char *name;
{	struct Symbol *t;
	++(sym->details.func.nargs);
	t = AddName( sym->details.func.locals, name );
	t->tag = PARAMETER;
	t->details.param.declaredin = sym;
	t->details.param.lastarg = NULL;
	sym->details.func.params = scons( t, sym->details.func.params );
};

/* Push and pop from the Symbol stack. See parser.y for how these
 * things are used.
 * Exported */
void PushSymbol( sym )
struct Symbol *sym;
{	if( ssp == SSTACKSIZE ) {
		errorstart();
		errorstr( "sstack overflow" );
		errorend();
		quit();
	};
	SymbolStack[ ssp++ ] = sym;
};

struct Symbol *PopSymbol()
{	if( ssp == 0 ) {
		errorstart();
		errorstr( "tstack underflow" );
		errorend();
		quit();
	};
	return( SymbolStack[ --ssp ] );
};

/* Note this sym as an extern on the current sym. Have to also not the 
 * creation of this pointer in erefedat, so that we can patch it when we
 * throw away this UNRESOLVED sym. */
static void MakeExtern( fromsym, sym )
struct Symbol *sym, *fromsym;
{	/* Add this to list of externs on the current symbol */
	fromsym->details.func.externs = scons( sym,
			fromsym->details.func.externs );
	/* And add this pointer to patch list for externals on sym */
	sym->erefedat = pcons( (int *) &fromsym->details.func.externs->this,
			sym->erefedat );
};

/* Moving symbols among tables ... the two ST's we are manipulating */
static struct SymbolTable *totable, *fromtable;
/* And the symbol we are moving UNRESOLVEDs from */
static struct Symbol *fromsym;

/* Given a sym on an internal table, transfer to the external table. 
 * If already exists, patch pointers to internal symbol to point to def 
 * in external table. If exists but is UNRESOLVED, merge two things. */
static void Resolve( sym )
struct Symbol *sym;
{	if( sym->tag == UNRESOLVED ) {
		struct Symbol *t;
		/* Add this to list of externs on the current symbol */
		MakeExtern( fromsym, sym );
		/* Look up name in outer table */
		t = LookTable( totable, sym->sname );
		if( t == NULL ) 
			/* Not there either .. pop the thing onto
			 * the external table */
			AddSym( totable, sym );
		else if( t->tag == UNRESOLVED ) {
			/* Also unresolved in external table. 
			 * Patch existing refs to point to this new
			 * UNRESOLVED & merge any new info onto
			 * external UNRES. */
			PatchRefs( sym->refedat, (int) t );
			t->refedat = pcat( sym->refedat, t->refedat );
			/* Patch any refs to sym in extern lists to
			 * point to this new UNRES. */
			PatchRefs( sym->erefedat, (int) t );
			/* Add extern patch list for the UNRES we are to
			 * throw away to the replacement UNRES. */
			t->erefedat = pcat( sym->erefedat, t->erefedat );
			/* No refs to old symbol left. Zap tag to make
			 * sure we spot any missed. */
			sym->tag = DEAD;
		}
		else {	/* Already exists on the external table ..
			 * Patch references to this sym to point
			 * to external sym */
			PatchRefs( sym->refedat, (int) t );
			/* Add extra info from symbol we will throw away */
			t->used = TRUE;
			/* Patch any refs to this symbol in extern lists */
			PatchRefs( sym->erefedat, (int) t );
			/* Throw away sym */
			sym->tag = DEAD;
		};
		/* And delete (unlink really) from the internal table */
		DeleteSym( fromtable, sym );
	};
};			

/* Scan a table, transfering unfound names to the ST one level up. See the
 * comment at the top of this file & parser.y 
 * Exported */
void ResolveNames( ex, in, sym )
struct SymbolTable *ex, *in;
struct Symbol *sym;
{	totable = ex;		/* Set up statics Resolve looks at */
	fromtable = in;
	fromsym = sym;
	ForAllSym( in, Resolve );
};
