/* Asp applicative langauge
 * J.Cupitt, November 86
 * The compiler. Necessary fuzzy int/pointer stuff hated by lint. */

#include "asphdr.h"			/* Get global header */
#include "evaltypes.h"			/* Runtime types */
#include "lextypes.h"			/* Needed by symboltypes */
#include "parsetypes.h"			/* Needed by symboltypes */
#include "symboltypes.h"		/* Get types we manipulate */
#include "error.h"			/* Need error handling */
#include "io.h"				/* Need line numbers */
#include "memory.h"			/* Need memory allocation */
#include "main.h"			/* Need command line flags */
#include "dump.h"			/* Get prettyprinter */
#include "symbol.h"			/* Need symbols. */
#include "declare.h"			/* Need ScriptRoot */
#include "parser.h"			/* Need root table */
#include "main.h"			/* We issue a warning! */
#include "list.h"			/* List handling macros */
#include "pointermacros.h"		/* Pointer following macro */

#define INFINITY 2000000		/* A really big number. */

/* Installing a CompileExpression result.
 * CompileExpression returns a Pointer --- if it has built a fragment
 * of graph, this will be an ABSOLUTE pointer to the root of that
 * graph. Constants are returned as NUM, CHAR etc.
 * This function is called to install the result of CompileExpression
 * once we know where it is going. We can then make RELATIVE pointers.
 * Also tweak reference counts. */
static void Install( dest, src )
struct Pointer *dest;
struct Pointer src;
{	if( src.tag == ABSOLUTE ) {
		/* Build a RELATIVE */
		dest->tag = RELATIVE;
		dest->data = SUB(src.data, dest);
		ADDREF(&src);
		return;
	} 
	else if( src.tag == RELATIVE ) {
		/* Eh ?!? */
		errorstart();
		errorstr( "broken in Install" );
		errorend();
		quit();
	}
	else
		/* A constant probably */
		*dest = src;
};

/* Like install, but make an ABSOLUTE rather than a RELATIVE pointer. */
static void InstallAbs( dest, src )
struct Pointer *dest;
struct Pointer src;
{	*dest = src;
	ADDREF(&src);
};

/* Compile an expression. 
 * Return a tagged pointer to the code we have produced. May be unboxed! 
 * Return absolute pointers, turn results of recursive calls into relative
 * pointers. */
static struct Pointer CompileExpression( expr )
struct ExpressionNode *expr;
{	struct Pointer res;		/* Build return result in this */
	switch( expr->tag ) {
		/* Compile unary operators */
		case CODE:
		case READ:
		case DECODE:
		case ERROR:
		case UNARYMINUS:
		case NOT:
		case HD:
		case TL:		
			{	struct EvalNode *t;
				/* Allocate our application cell */
				t = GetCell();
				/* Note address in codeat field */
				expr->codeat = t;
				/* Set up t */
				t->tag = expr->tag;
				t->refcount = 0;
				/* Compile argument */
				Install( &t->left, 
					CompileExpression( 
						expr->details.thisop ) );
				/* Set up return pointer */
				res.tag = ABSOLUTE;
				res.data = (unsigned) t;
				return( res );
			};
		case IDENTITY:		
			res.tag = ABSOLUTE;
			res.data = (unsigned) expr->details.thisop->codeat;
			return( res );
		case GEN1:	
			{	struct EvalNode *t;
				/* Allocate our application cell */
				t = GetCell();
				/* Note address in codeat field */
				expr->codeat = t;
				/* Set up t */
				t->tag = expr->tag;
				t->refcount = 0;
				/* Compile argument */
				Install( &t->left, 
					CompileExpression( 
						expr->details.gennode.gen1 ) );
				/* Set up return pointer */
				res.tag = ABSOLUTE;
				res.data = (unsigned) t;
				return( res );
			};
		/* Compile binary operators */
		case WRITE:
		case AND:
		case OR:
		case CONS:	
		case BINARYPLUS:	
		case BINARYMINUS:
		case TIMES:	
		case DIVIDE:
		case LESS:		
		case MORE:	
		case EQUAL:
		case MOREEQUAL:		
		case LESSEQUAL:	
		case NOTEQUAL:		
		case APPLICATION:	
			{	struct EvalNode *t;
				/* Allocate our application cell */
				t = GetCell();
				/* Note address in codeat field */
				expr->codeat = t;
				/* Set up t */
				t->tag = expr->tag;
				t->refcount = 0;
				/* Compile arguments */
				Install( &t->left, 
					CompileExpression( 
						expr->details.binop.leftop ) );
				Install( &t->right, 
					CompileExpression( 
					       expr->details.binop.rightop ) );
				/* Set up return pointer */
				res.tag = ABSOLUTE;
				res.data = (unsigned) t;
				return( res );
			}
		/* Compile function composition .. we need to generate a tiny
		 * lambda here. f.g = \x.f(g(x)) */
		case COMP:
			{	struct EvalNode *u, *v;
				struct EvalNode *s, *t;
				struct Pointer p;
				/* Get the cells we will fill in */
				u = GetCell(); v = GetCell();
				/* Note the address */
				expr->codeat = u;
				/* Fill in fields on first node */
				u->tag = LAMBDA;
				u->refcount = 0;
				/* Fix extent value later */
				u->left.tag = EXTENT;
				/* Set the fields in the second node */
				v->tag = APPLICATION; /* Why not ?? */
				v->right.tag = NIL;
				v->refcount = 0;
				/* Link the two nodes together */
				p.tag = ABSOLUTE;
				p.data = (unsigned) v;
				Install( &u->right, p );
				/* Make the f( .. ) part of the application */
				s = GetCell();
				s->tag = APPLICATION;
				s->refcount = 1;
				Install( &s->left, 
					CompileExpression( 
						expr->details.binop.leftop ) );
				/* Make the g(x) part of the application */
				t = GetCell();
				t->tag = APPLICATION;
				t->refcount = 0;
				t->right.tag = NIL;
				Install( &t->left, 
					CompileExpression( 
						expr->details.binop.rightop ) );
				/* Link s,t together */
				p.tag = ABSOLUTE;
				p.data = (unsigned) t;
				Install( &s->right, p );
				/* Set the extent */
				u->left.data = (unsigned) (GetTop() - u - 2);
				/* Set a ptr to x */
				v->left.tag = FORMAL;
				v->left.data = (unsigned) 
					SUB(&t->right, &v->left );
				/* Set up return pointer */
				res.tag = ABSOLUTE;
				res.data = (unsigned) u;
				return( res );
			}
		case GEN2:		
		case GEN3:		
			{	struct EvalNode *t;
				/* Allocate our application cell */
				t = GetCell();
				/* Note address in codeat field */
				expr->codeat = t;
				/* Set up t */
				t->tag = expr->tag;
				t->refcount = 0;
				/* Compile arguments */
				Install( &t->left, 
					CompileExpression( 
						expr->details.gennode.gen1 ) );
				Install( &t->right, 
					CompileExpression( 
						expr->details.gennode.gen2 ) );
				/* Set up return pointer */
				res.tag = ABSOLUTE;
				res.data = (unsigned) t;
				return( res );
			};
		case VARREF:		
			res.tag = SOURCE;
			res.data = (unsigned) expr->details.ident;
			return( res );
		case IF:		
			{	struct EvalNode *t, *s;
				/* Allocate our cells */
				s = GetCell(); t = GetCell();
				/* Note address in codeat field */
				expr->codeat = s;
				/* Set up s,t */
				s->tag = IF;
				s->refcount = 0;
				t->tag = APPLICATION;
				t->refcount = 0;
				Install( &s->right, 
					CompileExpression( 
					    expr->details.ifnode.condition ) );
				Install( &t->left, 
					CompileExpression( 
					    expr->details.ifnode.thenpart ) );
				Install( &t->right, 
					CompileExpression( 
					    expr->details.ifnode.elsepart ) );
				res.tag = ABSOLUTE;
				res.data = (unsigned) t;
				Install( &s->left, res );
				res.data = (unsigned) s;
				return( res );
			};
		case GEN4:		
			{	struct EvalNode *t, *s;
				/* Allocate our cells */
				s = GetCell(); t = GetCell();
				/* Note address in codeat field */
				expr->codeat = s;
				/* Set up s,t */
				s->tag = GEN4;
				s->refcount = 0;
				t->tag = APPLICATION;
				t->refcount = 0;
				Install( &s->left, 
					CompileExpression( 
						expr->details.gennode.gen1 ) );
				Install( &t->left, 
					CompileExpression( 
						expr->details.gennode.gen2 ) );
				Install( &t->right, 
					CompileExpression( 
						expr->details.gennode.gen3 ) );
				res.tag = ABSOLUTE;
				res.data = (unsigned) t;
				Install( &s->right, res );
				res.data = (unsigned) s;
				return( res );
			};
		case CONSTANT:		
			res.tag = expr->details.cons->tag;
			switch( res.tag ) {
				case NUM:
					res.data = (unsigned) 
						expr->details.cons->
							details.intconst;
					break;
				case CHAR:
					res.data = (unsigned) 
						expr->details.cons->
							details.charconst;
					break;
				case BOOL:
					res.data = (unsigned) 
						expr->details.cons->
							details.boolconst;
					break;
				case NIL:
					break;
			};
			return( res );
		default:	
			errorstart();
			errorstr( "broken in CompileExpression" );
			errorend();
			quit();
	};
};

/* Make the lambdas to start a function off. Return a pointer to the head
 * lambda. Extents are fixed up later. The list of
 * locals we see is in the reverse order to that which God intended ..
 * have to walk to end of that first. */
static struct EvalNode *MakeLambdas( locals )
struct SymbolList *locals;
{	struct EvalNode *res;		/* Our result */
	struct EvalNode *u, *v;		/* Our lambda nodes */
	struct Symbol *s;		/* Our local */
	struct Pointer p;		/* For joining up nodes. */
	/* Easy case */
	if( locals == NULL ) 
		return( NULL );
	/* Do other lambdas first */
	res = MakeLambdas( tl(locals) );
	/* Do this lambda */
	u = GetCell(); v = GetCell();
	/* Fill in fields on first of lambda pair. count of one since this
	 * LAMBDA is probably preceded by another LAMBDA. Counts are
	 * fixed up properly in CompileFunction. */
	u->tag = LAMBDA;
	u->refcount = 1;
	/* Extents are fixed up later, but set tag now */
	u->left.tag = EXTENT;
	/* Not really an application, but will do */
	v->tag = APPLICATION;
	/* Count set by SetRelative later */
	v->refcount = 0;
	/* Left is substitution list */
	v->left.tag = NIL;
	/* Right is patch list */
	v->right.tag = NIL;		
	s = hd(locals);
	/* Set lastarg field in param to point to subst. list */
	s = hd(locals);
	s->details.param.lastarg = &v->left;
	/* Link two nodes together */
	p.tag = ABSOLUTE;
	p.data = (unsigned) v;
	Install( &u->right, p );
	if( tl(locals) == NULL ) 
		/* We are the last local in the list & therefore the first
		 * generated. Our position is the result of the function
		 * call. */
		return( u );
	else
		/* Return the result of the recursive call */
		return( res );	
};

/* Fix up lambda extents. Passed the address of the cell after the end 
 * of the function and the first lambda cell. */
static void FixExtents( locals, aftercell, thiscell )
struct SymbolList *locals;
struct EvalNode *aftercell, *thiscell;
{	if( locals == NULL )
		return;
	else {
		thiscell->left.data = (unsigned) (aftercell - thiscell - 2);
		FixExtents( tl(locals), 
			aftercell, 
			INDIRECTREL( &thiscell->right )+1 );
	};
};

/* Fix up external refs in a pointer. Used by LinkFunctions below */
static void LinkPointer( p )
struct Pointer *p;
{	struct Symbol *sym;
	if( p->tag != SOURCE )
		/* Not interesting */
		return;
	/* Find symbol */
	sym = INDIRECTSYM(p);
	if( sym->tag == TOPFUNC )
		/* Make an absolute pointer to function */
		InstallAbs( p, sym->details.func.code );
	else if( sym->tag == FUNCTION )
		/* A local function .. make a relative link */
		Install( p, sym->details.func.code );
	else {	/* End of the world! */
		errorstart();
		errorstr( "broken in LinkPointer" );
		errorend();
		quit();
	};
};

/* Scan the heap resolving external references */
static void LinkFunctions()
{	ForHeap( LinkPointer );
};
	
/* Is it in a SymbolList? */
static enum Boolean MemberSList( a, list )
struct Symbol *a;
struct SymbolList *list;
{	if( list == NULL )
		return( FALSE );
	else if( hd(list) == a )
		return( TRUE );
	else
		return( MemberSList( a, tl(list) ) );
};

/* Set by CompileFunction for the use of LinkLocals. List of the parameters
 * of the current function. */
static struct SymbolList *params;

/* Fix up locals in an expression. Called from ForAllCell.
 * Look at every Pointer, checking for SOURCEs. */
static void LinkLocals( p )
struct Pointer *p;
{	struct Symbol *arg;
	if( p->tag == SOURCE && (arg = INDIRECTSYM(p))->tag == PARAMETER 
		&& MemberSList( arg, params ) ) {
		/* It's a reference to a local of the function we are trying
		 * to compile. Add ourselves to the argument chain */
		struct Pointer *q;
		q = arg->details.param.lastarg;
		p->tag = NIL;
		q->tag = FORMAL;
		q->data = (unsigned) SUB(p, q);
		arg->details.param.lastarg = p;
	};
};

/* Set by CompileFunction for the use of BuildPatch. List of the functions
 * external to the current function. */
static struct SymbolList *externs;
/* Build the patch list on this. */
static struct EvalNode *patchlist;

/* Scan built code, looking for SOURCE pointers to functions on the externs
 * list for this function. Add another element to the patch list if we
 * find one. */
static void BuildPatch( point )
struct Pointer *point;
{	struct Symbol *arg;
	if( point->tag == SOURCE && (arg = INDIRECTSYM(point))->tag == FUNCTION
		&& MemberSList( arg, externs ) ) {
		/* Found one .. make a new element for the patch list */
		struct EvalNode *p;
		p = GetCell();
		/* Why not ?!? */
		p->tag = CONS;
		p->refcount = 0;
		p->left.tag = FORMAL;
		p->left.data = (unsigned) SUB(point, &p->left);
		if( patchlist == NULL ) 
			p->right.tag = NIL;
		else {
			/* Make p->right a relative pointer to the previous
			 * head of the patchlist. */
			struct Pointer q;
			q.tag = ABSOLUTE;
			q.data = (unsigned) patchlist;
			Install( &p->right, q );
		};
		patchlist = p;
	};
};

/* Compile a function. Leave external references unresolved - these
 * are fixed up at a later stage. Ignore unused function, but issue a warning.
 * Testing for unused functions is not really enough -- should discard funcs
 * in calling tree below unused func. (Or at least decrement counts). */
static void CompileFunction( sym )
struct Symbol *sym;
{	struct EvalNode *lams, *end;
	struct Pointer bod;
#ifdef DEBUG
	printf( "Compiling %s\n", sym->sname );
#endif
	if( !ISFUNC(sym) )
		/* Can't compile PARAMs! */
		return;
	/* No code for unrefed functions. */
	if( !sym->used ) {
		if( Warnings ) {
			errorstart();
			errorstr( "warning: function " );
			errorstr( sym->sname );
			errorstr( " declared at " );
			errornum( sym->declaredat );
			errorstr( ":" );
			errorstr( sym->fileat );
			errorstr( " but never used" );
			errorend();
		};
		return;
	};
	/* Check for 0-ary function .. not really a function. */
	if( sym->details.func.nargs == 0 ) {
		/* Not really a function at all .. just make some code for us,
		 * then recurse for local defns */
		sym->details.func.code = 
			CompileExpression( sym->details.func.body );
		ForAllSym( sym->details.func.locals, CompileFunction );
		return;
	};
	/* Defntly a function. Make the lambdas that start it off. */
	lams = MakeLambdas( sym->details.func.params );
	/* Compile the body */
	bod = CompileExpression( sym->details.func.body );
	/* May have an unboxed body ... as we have args, need to provide a
	 * minimal one. */
	if( !ISPOINTER(&bod) ) {
		/* Build an I node. */
		struct EvalNode *r;
		r = GetCell();
		r->refcount = 0;
		r->tag = IDENTITY;
		Install( &r->left, bod );
		bod.tag = ABSOLUTE;
		bod.data = (unsigned) r;
	};
	/* Need a count on head of body, for FreeMultiple. */
	ADDREF(&bod);
	/* Fix counts on lambdas .. if we have a TOPFUNC, give it a infinite
	 * ref count and mark as a HEADLAMBDA. Otherwise it's a local
	 * definition and requires a zero ref count. This count is
	 * subsequently set when we link up x refs in functions. */
	if( sym->tag == TOPFUNC ) {
		lams->tag = HEADLAMBDA;
		lams->refcount = INFINITY;
	}
	else	/* MakeLambdas leaves count as 1 on all lams. */
		lams->refcount = 0;
	/* Compile any sub-functions */
	ForAllSym( sym->details.func.locals, CompileFunction );
	/* Find the end */
	end = GetTop();
	/* Fix extents on our lambdas */
	FixExtents( sym->details.func.params, end, lams );
	/* Link up locals in the bit we have built. params is a static
	 * LinkLocals uses to see which function its on at the moment */
	params = sym->details.func.params;
	ForAllCell( INDIRECT(&bod), end, LinkLocals );
	/* Build the patch list. Scan the built code again, this time looking
	 * for SOURCE references to functions on the externs list of this
	 * function. */
	externs = sym->details.func.externs;
	patchlist = NULL;
	ForAllCell( INDIRECT(&bod), end, BuildPatch );
	if( patchlist != NULL ) {
		/* Link on patch list */
		struct Pointer q;
		q.tag = ABSOLUTE;
		q.data = (unsigned) patchlist;
		Install( &(lams+1)->right, q );
	};
	/* Link code onto symbol */
	sym->details.func.code.tag = ABSOLUTE;
	sym->details.func.code.data = (unsigned) lams;
#ifdef TREES
	printf( "Code is:\n" );
	DumpNode( 1, lams );
#endif
};	

/* Compile the program!
 * Exported */
void CompileTable()
{	/* Compile all those globals!! */
	ForAllSym( CurrentTable, CompileFunction );
	/* And link them together */
	LinkFunctions();
	/* One count extra on main (we start reducing here) */
	ADDREF(&ScriptRoot->details.func.code);
#ifdef TREES
	printf( "Graph is:\n" );
	DumpGraph( 1, &ScriptRoot->details.func.code );
#endif
};
