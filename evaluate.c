/* Asp applicative langauge
 * J.Cupitt, November 86
 * The evaluator. Necessary fuzzy int/pointer stuff hated by lint.  */

#include "asphdr.h"			/* Get global header */
#include "evaltypes.h"			/* Runtime types */
#include "lextypes.h"			/* Needed by symboltypes */
#include "parsetypes.h"			/* Needed by symboltypes */
#include "symboltypes.h"		/* Get types we manipulate */
#include "error.h"			/* Need error handling */
#include "io.h"				/* Need line numbers */
#include "memory.h"			/* Need memory allocation */
#include "dump.h"			/* Get prettyprinter */
#include "declare.h"			/* Need ScriptRoot */
#include "main.h"			/* Need stack defaults */
#include "list.h"			/* List handling macros */
#include "pointermacros.h"		/* Pointer following macro */

int NumberReductions = 0;		/* Number of reductions we do */

/* These two are exported to the memory handler. It needs to scan these during relocation */
struct EvalNode **Stack;		/* Runtime argument stack */
int sp;					/* Stack pointer */

static int spfence;			/* What we cannot reduce below. */
static int *FenceStack;			/* Stack of past fences */
static int fsp;				/* Fence stack */

/* Stack of pointers to C pointers into the heap. Exported to the memory .
 * relocator. This is why we can't have register variables all the time! */
struct EvalNode ***PointerStack;
int psp;

static struct Pointer Reduce();		/* Need to forward ref these */
static void PrettyLeft();
static void PrettyRight();
static enum Boolean EqualRightRight();
static enum Boolean EqualLeftLeft();
static void ForceLeft();
static void ForceRight();

#define MAXFILENAME 1024		/* Max length of name for READ */

/* Macros for argument stack manipulation. */
#define PUSH(A) {Stack[sp++]=(A);if(sp>AStackSize){errorstart();\
	errorstr( "arg stack full" ); errorend(); quit(); }}
#define POP (Stack[--sp])

/* Macros for manipulating the pointer stack. */
#define PPUSH(A) {PointerStack[psp++]=(&(A));if(psp>PStackSize){errorstart();\
	errorstr( "pstack full" ); errorend(); quit(); }}
#define PPOP (--psp)

/* More macros for pointer following etc. */
#define FRCELL(A) ((A)->tag=FREE)
#define FREENODE(A) ( ((--(A)->refcount)==0)?(FRCELL(A),TRUE):(FALSE) )
#define CALLIFPOINTER(A,B) {if((A)->tag==ABSOLUTE){(B)(INDIRECTABS(A));} else if((A)->tag==RELATIVE){(B)(INDIRECTREL(A));};}
#define REDPOINT(A) {if((A).tag==ABSOLUTE) (A)=Reduce(INDIRECTABS(&A)); else if((A).tag==RELATIVE) (A)=Reduce(INDIRECTREL(&A));}

/* Macros for fence stack manipulation */
#define PUSHFENCE {if(fsp>FStackSize){ errorstart();\
	errorstr( "fstack full" ); errorend(); quit(); };\
	FenceStack[fsp++]=spfence; spfence=sp; }
#define POPFENCE {if(sp != (spfence=FenceStack[--fsp])){\
	errorstart(); errorstr( "args unused" ); errorend(); quit(); };}

/* Making and clearing fences */
#define FENCERED(A) {if(sp!=spfence){PUSHFENCE;REDPOINT(A);POPFENCE;}\
	else REDPOINT(A);}

/* Error on stack allocation */
static void memerr()
{	errorstart();
	errorstr( "unable to allocate space for eval stacks" );
	errorend();
	quit();
};

/* Initialise the stacks we use. Sizes are set by main.c from CL flags.
 * Exported */
void InitEvalStacks()
{	Stack = (struct EvalNode **)
		malloc( AStackSize * sizeof(struct EvalNode *) );
	if( Stack == NULL )
		memerr();
	FenceStack = (int *)
		malloc( FStackSize * sizeof(int) );
	if( FenceStack == NULL )
		memerr();
	PointerStack = (struct EvalNode ***)
		malloc( PStackSize * sizeof(struct EvalNode **) );
	if( PointerStack == NULL )
		memerr();
	/* Clear all the stacks */
	sp = fsp = psp = spfence = 0;
};

/* Help with error messages */
static void typeerror( op, should )
char *op, *should;
{	errorstart();
	errorstr( "args to " );
	errorstr( op );
	errorstr( " should be " );
	errorstr( should );
	errorend();
	quit();
};

/* Free a node recursivly */
static void FreeMultiple( n )
register struct EvalNode *n;
{	/* No need to declare the EvalNode ... */
#ifdef DEBUG
	printf( "Freeing multiple ...\n" );
#endif
	if( --n->refcount == 0 )
		switch( n->tag ) {
			case GEN1:
			case READ:
			case ERROR:
			case CODE:
			case DECODE:
			case IDENTITY:	
			case UNARYMINUS:
			case NOT:
			case HD:
			case TL:
				CALLIFPOINTER(&n->left,FreeMultiple);
				FRCELL( n );
				return;
			case WRITE:
			case GEN2:
			case GEN3:
			case GEN4:
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
			case IF:		
			case APPLICATION:	
				CALLIFPOINTER(&n->left,FreeMultiple);
				CALLIFPOINTER(&n->right,FreeMultiple);
				FRCELL( n );
				return;
			case STREAM:		
				/* More complicated -- have to close
				 * the file. */
				{ 	FILE *fd;
					fd = (FILE *) n->left.data;
					fclose( fd );
					FRCELL( n );
					return;
				};
			case HEADLAMBDA:
			case LAMBDA:		
				FRCELL( n );
				FRCELL( INDIRECT( &n->right ) );
				CALLIFPOINTER( &INDIRECT(&n->right)->right,
					FreeMultiple );
				FreeMultiple( INDIRECT( &n->right )+1 );
				return;
			case FREE:
				/* May sometimes happen legally ... see
				 * EQUAL in Reduce. */
				return;
			default:
				errorstart();
				errorstr( "broken in FreeMultiple" );
				errorend();
				quit();
		};
};

/* Test two pointers for equality. Cannot common up structures if equal!
 * (would confuse reference counting). Pass two pointers to EvalNodes and
 * (in this version) test the LH pointers. Have to be careful about compaction
 * under our feet! */
static enum Boolean EqualLeftLeft( a, b )
struct EvalNode *a, *b;
{	register struct Pointer *c, *d;		/* Cache in here .. */
#ifdef DEBUG
	printf( "Starting EqualLeftLeft...\n" );
#endif
	PPUSH(a); PPUSH(b);			/* Register pointers */
	FENCERED(a->left);			/* Reduce the two pointers */
	FENCERED(b->left);
	PPOP; PPOP;
	c = &a->left; d = &b->left;		/* Read answer into regs */
	switch( c->tag ) {
		case NUM:	
			return( (enum Boolean) (
				(d->tag == NUM) &&
				(c->data == d->data) ) );
		case CHAR:
			return( (enum Boolean) (
				(d->tag == CHAR) &&
				((char) c->data == (char) d->data) ) );
		case BOOL:	
			return( (enum Boolean) (
				(d->tag == BOOL) &&
				((enum Boolean) c->data == 
					(enum Boolean) d->data) ) );
		case NIL:
			return( (enum Boolean) (
				(d->tag == NIL) ) );
		case ABSOLUTE:	
			if( ISPOINTER(d) ) {
				if( INDIRECTABS(c)->tag != CONS || 
					INDIRECT(d)->tag != CONS ) {
					errorstart();
					errorstr( "= applied to a function" );
					errorend();
					quit();
				};
				return( (enum Boolean) (
					EqualLeftLeft( INDIRECTABS(c), 
						INDIRECT(d) ) &&
					EqualRightRight( INDIRECTABS(c), 
						INDIRECT(d) ) 
					) );
			};
			return( FALSE );
		case RELATIVE:
			if( ISPOINTER(d) ) {
				if( INDIRECTREL(c)->tag != CONS || 
					INDIRECT(d)->tag != CONS ) {
					errorstart();
					errorstr( "= applied to a function" );
					errorend();
					quit();
				};
				return( (enum Boolean) (
					EqualLeftLeft( INDIRECTREL(c), 
						INDIRECT(d) ) &&
					EqualRightRight( INDIRECTREL(c), 
						INDIRECT(d) ) 
					) );
			};
			return( FALSE );
		default:
			errorstart();
			errorstr( "Broken in equal" );
			errorend();
			quit();
	};
};

/* Test two pointers for equality. Pass two pointers to EvalNodes and
 * (in this version) test the RH pointers. */
static enum Boolean EqualRightRight( a, b )
struct EvalNode *a, *b;
{	register struct Pointer *c, *d;		/* Cache in here .. */
#ifdef DEBUG
	printf( "Starting EqualRightRight ...\n" );
#endif
	PPUSH(a); PPUSH(b);			/* Register pointers */
	FENCERED(a->right);			/* Reduce the two pointers */
	FENCERED(b->right);
	PPOP; PPOP;
	c = &a->right; d = &b->right;		/* Read answer into regs */
	switch( c->tag ) {
		case NUM:
			return( (enum Boolean) (
				(d->tag == NUM) &&
				(c->data == d->data) ) );
		case CHAR:
			return( (enum Boolean) (
				(d->tag == CHAR) &&
				((char) c->data == (char) d->data) ) );
		case BOOL:
			return( (enum Boolean) (
				(d->tag == BOOL) &&
				((enum Boolean) c->data == 
					(enum Boolean) d->data) ) );
		case NIL:
			return( (enum Boolean) (
				(d->tag == NIL) ) );
		case ABSOLUTE:	
			if( ISPOINTER(d) ) {
				if( INDIRECTABS(c)->tag != CONS || 
					INDIRECT(d)->tag != CONS ) {
					errorstart();
					errorstr( "= applied to a function" );
					errorend();
					quit();
				};
				return( (enum Boolean) (
					EqualLeftLeft( INDIRECTABS(c), 
						INDIRECT(d) ) &&
					EqualRightRight( INDIRECTABS(c), 
						INDIRECT(d) ) 
					) );
			};
			return( FALSE );
		case RELATIVE:
			if( ISPOINTER(d) ) {
				if( INDIRECTREL(c)->tag != CONS || 
					INDIRECT(d)->tag != CONS ) {
					errorstart();
					errorstr( "= applied to a function" );
					errorend();
					quit();
				};
				return( (enum Boolean) (
					EqualLeftLeft( INDIRECTREL(c), 
						INDIRECT(d) ) &&
					EqualRightRight( INDIRECTREL(c), 
						INDIRECT(d) ) 
					) );
			};
			return( FALSE );
		default:
			errorstart();
			errorstr( "Broken in equal" );
			errorend();
			quit();
	};
};

/* Test two pointers for equality. Pass two pointers to EvalNodes and
 * (in this version) test the LH against the RH. Called only from Reduce. */
static enum Boolean EqualLeftRight( a, b )
struct EvalNode *a, *b;
{	register struct Pointer *c, *d;		/* Cache in here .. */
#ifdef DEBUG
	printf( "Starting EqualLeftRight ...\n" );
#endif
	PPUSH(a); PPUSH(b);			/* Register pointers */
	FENCERED(a->left);			/* Reduce the two pointers */
	FENCERED(b->right);
	PPOP; PPOP;
	c = &a->left; d = &b->right;		/* Read answer into regs */
	switch( c->tag ) {
		case NUM:
			return( (enum Boolean) (
				(d->tag == NUM) &&
				(c->data == d->data) ) );
		case CHAR:
			return( (enum Boolean) (
				(d->tag == CHAR) &&
				((char) c->data == (char) d->data) ) );
		case BOOL:
			return( (enum Boolean) (
				(d->tag == BOOL) &&
				((enum Boolean) c->data == 
					(enum Boolean) d->data) ) );
		case NIL:
			return( (enum Boolean) (
				(d->tag == NIL) ) );
		case ABSOLUTE:	
			if( ISPOINTER(d) ) {
				if( INDIRECTABS(c)->tag != CONS || 
					INDIRECT(d)->tag != CONS ) {
					errorstart();
					errorstr( "= applied to a function" );
					errorend();
					quit();
				};
				return( (enum Boolean) (
					EqualLeftLeft( INDIRECTABS(c), 
						INDIRECT(d) ) &&
					EqualRightRight( INDIRECTABS(c), 
						INDIRECT(d) ) 
					) );
			};
			return( FALSE );
		case RELATIVE:	
			if( ISPOINTER(d) ) {
				if( INDIRECTREL(c)->tag != CONS || 
					INDIRECT(d)->tag != CONS ) {
					errorstart();
					errorstr( "= applied to a function" );
					errorend();
					quit();
				};
				return( (enum Boolean) (
					EqualLeftLeft( INDIRECTREL(c), 
						INDIRECT(d) ) &&
					EqualRightRight( INDIRECTREL(c), 
						INDIRECT(d) ) 
					) );
			};
			return( FALSE );
		default:
			errorstart();
			errorstr( "Broken in equal" );
			errorend();
			quit();
	};
};

/* Force evaluation of a structure. Used by things like READ that are strict
 * in some structured argument. */
static void ForceLeft( stacknode )
struct EvalNode *stacknode;
{	PPUSH(stacknode);
	FENCERED(stacknode->left);
	if( ISPOINTER(&stacknode->left) ) {
		stacknode = INDIRECT(&stacknode->left);
		if( stacknode->tag == CONS ) {
			ForceLeft( stacknode );
			ForceRight( stacknode );
		};
	};
	PPOP;
};

/* Force evaluation of a structure. Used by things like READ that are strict
 * in some structured argument. */
static void ForceRight( stacknode )
struct EvalNode *stacknode;
{	PPUSH(stacknode);
	FENCERED(stacknode->right);
	if( ISPOINTER(&stacknode->right) ) {
		stacknode = INDIRECT(&stacknode->right);
		if( stacknode->tag == CONS ) {
			ForceLeft( stacknode );
			ForceRight( stacknode );
		};
	};
	PPOP;
};

/* Walk a structure, building a C string. Used by things like READ and WRITE
 * that take a file name as argument. Pass an EvalNode, start from it's
 * LHS. Also pass a string to be used as an error message in case we hit
 * a non-char.
 * Assume the buffer passed is at least MAXFILENAME long. */
static void GetStringLeft( stacknode, name, err )
struct EvalNode *stacknode;
register char *name;
char *err;
{	/* Length of string collected so far */
	register int p = 0;
	/* Cache node pointers in here */
	register struct EvalNode *node;
#ifdef DEBUG
	printf( "Reducing [char]\n" );
#endif
	/* Force evaluation of the argument */
	PPUSH(stacknode);
	ForceLeft( stacknode );
	PPOP;
	node = stacknode;
	/* Walk down the arg, building a C string. */
	if( !ISPOINTER(&node->left) )
		typeerror( err, "list" );
	node = INDIRECT(&node->left);
	if( node->tag != CONS )
		typeerror( err, "list" );
	for(;;) {
		if( node->left.tag != CHAR )
			typeerror( err, "list of char" );
		name[p++] = GETCHAR(&node->left);
		if( p > MAXFILENAME ) {
			errorstart();
			errorstr( err );
			errorstr( " arg too long" );
			errorend();
			quit();
		};
		if( node->right.tag == NIL ) {
			name[p] = '\0';
			break;
		};
		if( !ISPOINTER(&node->right) )
			typeerror( err, "list" );
		node = INDIRECT(&node->right);
		if( node->tag != CONS )
			typeerror( err, "list" );
	};
};

/* Evaluate a node. Return a Pointer containing an unboxed value, a pointer 
 * to a lambda or a pointer to a cons node. Free up the node (unless we are 
 * returning a cons)
 * after reducing it. Caller relinquishes all rights to node being reduced! 
 * Have to look out for compaction. We keep two copies of our node pointer --
 * one on the stack and one in a register. Refresh the register everytime
 * there is a chance the stack may have changed. */
static struct Pointer Reduce( stacknode )
struct EvalNode *stacknode;
{	register struct EvalNode *node;
	/* Loop here in some cases (elimination of tail recursive calls)... */
mainloop:
#ifdef TREES
	printf( "Graph on entry to Reduce\n" );
	DumpGraph( 2, &ScriptRoot->details.func.code );
#endif
	++NumberReductions;	/* Note this reduction */
	switch( (node = stacknode)->tag ) {
		case IDENTITY:
#ifdef DEBUG
			printf( "Identity\n" );
#endif
			/* goto here at end of HD, TL, IF -- anywhere that can
			 * create an I node yielding the result. */
doInode:
			PPUSH(stacknode);
			REDPOINT( stacknode->left );
			PPOP;
			node = stacknode;
			/* If we are unable to free ourselves after 
			 * decrementing our count, we must up count on arg. 
			 * as we are creating an extra pointer. */
			if( !FREENODE( node ) )
				ADDREF( &node->left );
			return( node->left );
		case GEN1:
			{ register struct EvalNode *newnode;
			  register int n;
			  PPUSH(stacknode);
			  newnode = GetBlock( 1 );
			  FENCERED(stacknode->left);
			  PPOP;
			  node = stacknode;
			  if( node->left.tag != NUM )
				typeerror( "[<n>..]", "num" );
			  n = GETINT(&node->left);
			  /* Link them up */
			  node->right.tag = ABSOLUTE;
			  node->right.data = (unsigned) newnode;
			  node->left.tag = NUM;
			  node->left.data = (unsigned) n;
			  node->tag = CONS;
			  newnode->tag = GEN1;
			  newnode->left.tag = NUM;
			  newnode->left.data = (unsigned) n+1;
			  newnode->refcount = 1;
			  goto consnode;
			};
		case GEN2:
			{ register struct EvalNode *newnode;
			  register int n, lim;
			  PPUSH(stacknode);
			  FENCERED(stacknode->left);
			  FENCERED(stacknode->right);
			  PPOP;
			  node = stacknode;
			  if( node->left.tag != NUM )
				typeerror( "[<n>..<n>]", "num" );
			  if( node->right.tag != NUM )
				typeerror( "[<n>..<n>]", "num" );
			  n = GETINT(&node->left);
			  lim = GETINT(&node->right);
			  if( n>lim ) {
				/* Time to go! Return [] */
				node->left.tag = NIL;
				if( !FREENODE(node) )
					node->tag = IDENTITY;
				return( node->left );
			  }
			  else {
				PPUSH(stacknode);
				newnode = GetBlock( 1 );
				PPOP;
				node = stacknode;
			  	/* Link them up */
			  	node->right.tag = ABSOLUTE;
			  	node->right.data = (unsigned) newnode;
			  	node->left.tag = NUM;
			  	node->left.data = (unsigned) n;
			  	node->tag = CONS;
			  	newnode->tag = GEN2;
			  	newnode->left.tag = NUM;
			  	newnode->left.data = (unsigned) n+1;
			  	newnode->right.tag = NUM;
			  	newnode->right.data = (unsigned) lim;
			  	newnode->refcount = 1;
			  	goto consnode;
			  };
			};
		case GEN3:
			{ register struct EvalNode *newnode;
			  register int n, step;
			  PPUSH(stacknode);
			  newnode = GetBlock( 1 );
			  FENCERED(stacknode->left);
			  FENCERED(stacknode->right);
			  PPOP;
			  node = stacknode;
			  if( node->left.tag != NUM )
				typeerror( "[<n>,<n>..]", "num" );
			  if( node->right.tag != NUM )
				typeerror( "[<n>,<n>..]", "num" );
			  n = GETINT(&node->left);
			  step = GETINT(&node->right);
			  /* Link them up */
			  node->right.tag = ABSOLUTE;
			  node->right.data = (unsigned) newnode;
			  node->left.tag = NUM;
			  node->left.data = (unsigned) n;
			  node->tag = CONS;
			  newnode->tag = GEN3;
			  newnode->left.tag = NUM;
			  newnode->left.data = (unsigned) step;
			  newnode->right.tag = NUM;
			  newnode->right.data = (unsigned) step+(step-n);
			  newnode->refcount = 1;
			  goto consnode;
			};
		case GEN4:
			{ register struct EvalNode *newnode;
			  register int n, step, lim, off;
			  struct EvalNode *stacknode2;
			  stacknode2 = INDIRECT(&node->right);
			  PPUSH(stacknode); PPUSH(stacknode2);
			  FENCERED(stacknode2->left);
			  FENCERED(stacknode2->right);
			  FENCERED(stacknode->left);
			  PPOP; PPOP;
			  node = stacknode;
			  if( node->left.tag != NUM )
				typeerror( "[<n>,<n>..<n>]", "num" );
			  n = GETINT(&node->left);
			  node = stacknode2;
			  if( node->left.tag != NUM )
				typeerror( "[<n>,<n>..<n>]", "num" );
			  if( node->right.tag != NUM )
				typeerror( "[<n>,<n>..<n>]", "num" );
			  step = GETINT(&node->left);
			  lim = GETINT(&node->right);
			  off = step-n;
			  if( (off>0 && n>lim) || (off<0 && n<lim) ) {
				/* Time to go! Return [] */
				FRCELL(node);
				node = stacknode;
				node->left.tag = NIL;
				if( !FREENODE(node) )
					node->tag = IDENTITY;
				return( node->left );
			  }
			  else {
				PPUSH(stacknode); PPUSH(stacknode2);
				newnode = GetBlock( 1 );
				PPOP; PPOP;
				node = stacknode;
			  	/* Link them up */
			  	node->right.tag = ABSOLUTE;
			  	node->right.data = (unsigned) newnode;
			  	node->left.tag = NUM;
			  	node->left.data = (unsigned) n;
			  	node->tag = CONS;
			  	newnode->tag = GEN4;
			  	newnode->left.tag = NUM;
			  	newnode->left.data = (unsigned) step;
			  	newnode->right.tag = ABSOLUTE;
			  	newnode->right.data = (unsigned) stacknode2;
			  	newnode->refcount = 1;
				stacknode2->left.data = (unsigned) step+off;
			  	goto consnode;
			  };
			};
		case STREAM:
			/* goto here from end of READ */
streamnode:
#ifdef DEBUG
			printf( "Reducing STREAM\n" );
#endif DEBUG
			{ FILE *fd;
			  int ch;
			  fd = (FILE *) node->left.data;
			  ch = getc( fd );
			  if( ch == EOF ) {
				/* Tidy up */
#ifdef DEBUG
				printf( "STREAM returns []\n" );
#endif DEBUG
				fclose( fd );
				/* Return NIL. May need to make an I node */
				node->left.tag = NIL;
				if( !FREENODE(node) )
					node->tag = IDENTITY;
				return( node->left );
			  }
			  else {
				/* Make a new stream node */
				register struct EvalNode *newnode;
#ifdef DEBUG
				printf( "STREAM returns %c\n", ch );
#endif DEBUG
				PPUSH(stacknode);
				newnode = GetBlock( 1 );
				PPOP;
				node = stacknode;
				/* Fill in fields and link it on */
				newnode->tag = STREAM;
				newnode->refcount = 1;
				newnode->left.data = (unsigned) fd;
				node->right.tag = ABSOLUTE;
				node->right.data = (unsigned) newnode;
				/* oldnode => cons */
				node->tag = CONS;
				/* Note character */
				node->left.tag = CHAR;
				node->left.data = (unsigned) ch;
				/* Resume from CONS */
				goto consnode;
			  };
			}; 
		case ERROR:
#ifdef DEBUG
			printf( "Hit error node\n" );
#endif
			fprintf( stderr, "User error - " );
			PrettyLeft( stderr, node );
			fprintf( stderr, "\n" );
			quit();
		case READ:
			{ char name[ MAXFILENAME ];
#ifdef DEBUG
			  printf( "Reducing READ\n" );
#endif
			  /* Read argument */
			  PPUSH(stacknode);
			  GetStringLeft( node, name, "read" );
			  PPOP;
			  node = stacknode;
			  /* Free up the arg */
			  FreeMultiple( INDIRECT(&node->left) );
			  /* Make ourselves into a STREAM node */
			  node->tag = STREAM;
			  node->left.data = (unsigned) fopen( name, "r" );
			  if( node->left.data == NULL ) {
				errorstart();
				errorstr( "read of '" );
				errorstr( name );
				errorstr( "' fails" );
				errorend();
				quit();
			  };
			  /* Continue reduction from STREAM */
			  goto streamnode;
			};
		case WRITE:
			{ char name[ MAXFILENAME ];
			  FILE *fd;
#ifdef DEBUG
			  printf( "Reducing WRITE\n" );
#endif
			  /* Read argument */
			  PPUSH(stacknode);
			  GetStringLeft( node, name, "read" );
			  node = stacknode;
			  /* Free up the name */
			  FreeMultiple( INDIRECT(&node->left) );
			  fd = fopen( name, "w" );
			  if( fd == NULL ) {
				errorstart();
				errorstr( "write to '" );
				errorstr( name );
				errorstr( "' fails" );
				errorend();
				quit();
			  };
			  /* Write the RHS expression */
			  PPUSH(stacknode);
			  PrettyRight( fd, node );
			  PPOP;
			  node = stacknode;
			  /* Free RHS up */
			  CALLIFPOINTER( &node->right, FreeMultiple );
			  /* Try to free ourselves */
			  if( !FREENODE( node ) )
				node->tag = IDENTITY;
			  node->left.tag = BOOL;
			  node->left.data = (unsigned) TRUE;
			  return( node->left );
			};
		case CODE:
#ifdef DEBUG
			printf( "Reducing code\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );
			PPOP;
			node = stacknode;
			if( node->left.tag != CHAR )
				typeerror( "code", "char" );
			node->left.data = (unsigned) GETCHAR(&node->left);
			node->left.tag = NUM;
#ifdef DEBUG
			printf( "Result of CODE is %d\n", GETINT(&node->left) );
#endif
			/* No need for fancy count twiddling on any of these
			 * primitives, as we can be sure of never creating a 
			 * boxed result. */
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case DECODE:
#ifdef DEBUG
			printf( "Reducing decode\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left )
			PPOP;
			node = stacknode;
			if( node->left.tag != NUM ) 
				typeerror( "decode", "char" );
			node->left.data = (unsigned) GETINT(&node->left);
			node->left.tag = CHAR;
#ifdef DEBUG
			printf( "Result of DECODE is %c\n", 
				GETCHAR(&node->left) );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case UNARYMINUS:
#ifdef DEBUG
			printf( "Reducing unary minus\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );
			PPOP;
			node = stacknode;
			if( node->left.tag != NUM ) 
				typeerror( "unary minus", "num" );
			node->left.data = (unsigned) -GETINT(&node->left);
#ifdef DEBUG
			printf( "Result of UNARYMINUS is %d\n", 
				GETINT(&node->left) );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case NOT:
#ifdef DEBUG
			printf( "Reducing not\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );
			PPOP;
			node = stacknode;
			if( node->left.tag != BOOL )
				typeerror( "~", "bool" );
			node->left.data = (unsigned) !GETBOOL(&node->left);
#ifdef DEBUG
			printf( "Result of NOT is %s\n", 
				GETBOOL(&node->left)?"true":"false" );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case HD:
			{ register struct Pointer *p;
			  register struct EvalNode *cons;
			  /* No need to PPUSH these two. */
#ifdef DEBUG
			  printf( "Reducing hd\n" );
#endif
			  PPUSH(stacknode);
			  FENCERED( stacknode->left );
			  PPOP;
			  node = stacknode;
			  if( !ISPOINTER(&node->left) ) 
				typeerror( "hd", "list" );
			  /* Get a ptr to the cons node */
			  cons = INDIRECTABS( &node->left );
			  if( cons->tag != CONS )
				typeerror( "hd", "list" );
			  /* Get a ptr to the head */
			  p = &cons->left;
			  /* Make ourselves into an I node */
			  node->tag = IDENTITY;
			  /* Copy the head up */
			  UNREL(p);
			  node->left = *p;
			  /* Counts:
			   * 	- One less on CONS node
			   * 	- One more on head of cons. */
			  if( !FREENODE(cons) )
				{ ADDREF(p) }
			  else
				CALLIFPOINTER(&cons->right,FreeMultiple);
			  goto doInode;
			};
		case TL:
			{ register struct Pointer *p;
			  register struct EvalNode *cons;
			  /* Again, no need to PPUSH. */
#ifdef DEBUG
			  printf( "Reducing tl\n" );
#endif
			  PPUSH(stacknode);
			  FENCERED( stacknode->left );
			  PPOP;
			  node = stacknode;
			  if( !ISPOINTER(&node->left) )
				typeerror( "tl", "list" );
			  /* Get a ptr to the cons node */
			  cons = INDIRECTABS( &node->left );
			  if( cons->tag != CONS )
				typeerror( "tl", "list" );
			  /* Get a ptr to the tail */
			  p = &cons->right;
			  /* Make ourselves into an I node */
			  node->tag = IDENTITY;
			  /* Copy the tail up */
			  UNREL(p);
			  node->left = *p;
			  /* Counts:
			   * 	- One less on CONS node
			   * 	- One more on tail of cons. */
			  if( !FREENODE(cons) )
				{ ADDREF(p); }
			  else
				CALLIFPOINTER(&cons->left,FreeMultiple);
			  goto doInode;
			};
		case CONS:
			/* goto here after STREAM node */
consnode:
			{ register struct Pointer s;
#ifdef DEBUG
			  printf( "Skipping cons\n" );
#endif
			  /* Despite creating an extra pointer here, we do 
			   * not need to increment any counts! 
			   * See HD,TL,I etc. */
			  s.tag = ABSOLUTE;
			  s.data = (unsigned) node;	
			  return( s );	
			};
		case AND:
#ifdef DEBUG
			printf( "Reducing and\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->right );
			node = stacknode;
			if( node->right.tag != BOOL )
				typeerror( "and", "bool" );
			if( GETBOOL(&node->right) ) {
				FENCERED( stacknode->left );
				PPOP;
				node = stacknode;
				if( node->left.tag != BOOL )
					typeerror( "and", "bool" );
			}
			else {	/* Free up side we ignore */
				PPOP;
				CALLIFPOINTER(&node->left,FreeMultiple);
				node->left.tag = BOOL;
				node->left.data = (unsigned) FALSE;
			};
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case OR:
#ifdef DEBUG
			printf( "Reducing or\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->right );
			node = stacknode;
			if( node->right.tag != BOOL )
				typeerror( "or", "bool" );
			if( !GETBOOL(&node->right) ) {
				FENCERED( node->left );
				PPOP;
				node = stacknode;
				if( node->left.tag != BOOL )
					typeerror( "or", "bool" );
			}
			else {	/* Free up side we ignore */
				PPOP;
				CALLIFPOINTER(&node->left,FreeMultiple)
				node->left.tag = BOOL;
				node->left.data = (unsigned) TRUE;
			};
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case BINARYPLUS:
#ifdef DEBUG
			printf( "Reducing binary plus\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag != NUM || node->right.tag != NUM )
				typeerror( "+", "num" );
			node->left.data = (unsigned) (
				GETINT(&node->left) + GETINT(&node->right));
#ifdef DEBUG
			printf( "Result of BINARYPLUS is %d\n", 
				GETINT(&node->left) );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case BINARYMINUS:
#ifdef DEBUG
			printf( "Reducing binary minus\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );	
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag != NUM || node->right.tag != NUM )
				typeerror( "binary -", "num" );
			node->left.data = (unsigned) (
				GETINT(&node->left) - GETINT(&node->right));
#ifdef DEBUG
			printf( "Result of BINARYMINUS is %d\n", 
				GETINT(&node->left) );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case TIMES:	
#ifdef DEBUG
			printf( "Reducing times\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );	
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag != NUM || node->right.tag != NUM )
				typeerror( "*", "num" );
			node->left.data = (unsigned) (
				GETINT(&node->left) * GETINT(&node->right));
#ifdef DEBUG
			printf( "Result of TIMES is %d\n", 
				GETINT(&node->left) );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case DIVIDE:
#ifdef DEBUG
			printf( "Reducing divide\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );	
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag != NUM || node->right.tag != NUM ) 
				typeerror( "/", "num" );
			if( GETINT(&node->right) == 0 ) {
				errorstart();
				errorstr( "Division by zero" );
				errorend();
				quit();
			};
			node->left.data = (unsigned) (
				GETINT(&node->left) / GETINT(&node->right));
#ifdef DEBUG
			printf( "Result of DIVIDE is %d\n", 
				GETINT(&node->left) );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case LESS:
#ifdef DEBUG
			printf( "Reducing less\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );	
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag == NUM && node->right.tag == NUM )
				node->left.data = (unsigned) (
					(GETINT(&node->left) < 
					GETINT(&node->right)));
			else if( node->left.tag == CHAR && node->right.tag == CHAR )
				node->left.data = (unsigned) (
					(GETCHAR(&node->left) < 
					GETCHAR(&node->right)));
			else 
				typeerror( "<", "num or char" );
			node->left.tag = BOOL;
#ifdef DEBUG
			printf( "Result of LESS is %s\n",
				GETBOOL(&node->left)?"true":"false" );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case MORE:	
#ifdef DEBUG
			printf( "Reducing more\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );	
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag == NUM && node->right.tag == NUM )
				node->left.data = (unsigned) (
					(GETINT(&node->left) > 
					GETINT(&node->right)));
			else if( node->left.tag == CHAR && node->right.tag == CHAR )
				node->left.data = (unsigned) (
					(GETCHAR(&node->left) >
					GETCHAR(&node->right)));
			else 
				typeerror( ">", "num or char" );
			node->left.tag = BOOL;
#ifdef DEBUG
			printf( "Result of MORE is %s\n", 
				GETBOOL(&node->left)?"true":"false" );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case EQUAL:
			{ register enum Boolean res;
#ifdef DEBUG
			  printf( "Reducing equal\n" );
#endif
			  PPUSH(stacknode);
			  res = EqualLeftRight( node, node );
			  PPOP;
			  node = stacknode;
#ifdef DEBUG
			  printf( "Result of EQUAL is %s\n", res ? "true":"false" );
#endif
			  /* Unlike any of the other comparision primitives, 
			   * we may be comparing structures - we must also 
			   * free these up if we can free ourselves. */
			  if( FREENODE( node ) ) {
				CALLIFPOINTER(&node->left,FreeMultiple);
				CALLIFPOINTER(&node->right,FreeMultiple);
			  }
			  else
				node->tag = IDENTITY;
			  node->left.tag = BOOL;
			  node->left.data = (unsigned) res;
			  return( node->left );
			};
		case MOREEQUAL:		
#ifdef DEBUG
			printf( "Reducing greater than or equal to\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );	
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag == NUM && node->right.tag == NUM )
				node->left.data = (unsigned) (
					(GETINT(&node->left) >= 
					GETINT(&node->right)));
			else if( node->left.tag == CHAR && node->right.tag == CHAR )
				node->left.data = (unsigned) (
					(GETCHAR(&node->left) >= 
					GETCHAR(&node->right)));
			else 
				typeerror( ">=", "num or char" );
			node->left.tag = BOOL;
#ifdef DEBUG
			printf( "Result of MOREEQUAL is %s\n", 
				GETBOOL(&node->left)?"true":"false" );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case LESSEQUAL:	
#ifdef DEBUG
			printf( "Reducing less than or equal to\n" );
#endif
			PPUSH(stacknode);
			FENCERED( stacknode->left );	
			FENCERED( stacknode->right );
			PPOP;
			node = stacknode;
			if( node->left.tag == NUM && node->right.tag == NUM )
				node->left.data = (unsigned) (
					(GETINT(&node->left) <= 
					GETINT(&node->right)));
			else if( node->left.tag == CHAR && node->right.tag == CHAR )
				node->left.data = (unsigned) (
					(GETCHAR(&node->left) <= 
					GETCHAR(&node->right)));
			else 
				typeerror( "<=", "num or char" );
			node->left.tag = BOOL;
#ifdef DEBUG
			printf( "Result of LESSEQUAL is %s\n", 
				GETBOOL(&node->left)?"true":"false" );
#endif
			if( !FREENODE( node ) )
				node->tag = IDENTITY;
			return( node->left );
		case NOTEQUAL:
			{ enum Boolean res;
#ifdef DEBUG
			  printf( "Reducing not equal\n" );
#endif
			  PPUSH(stacknode);
			  res = (enum Boolean) !EqualLeftRight( node, node );
			  PPOP;
			  node = stacknode;
#ifdef DEBUG
			  printf( "Result of NOTEQUAL is %s\n", 
				res ? "true":"false" );
#endif
			  /* Unlike any of the other comparision primitives, 
			   * we may be comparing structures - we must also 
			   * free these up if we can free ourselves. */
			  if( FREENODE( node ) ) {
				CALLIFPOINTER(&node->left,FreeMultiple);
				CALLIFPOINTER(&node->right,FreeMultiple);
			  }
			  else
				node->tag = IDENTITY;
			  node->left.tag = BOOL;
			  node->left.data = (unsigned) res;
			  return( node->left );
			};
		case IF:
			{ register struct EvalNode *code;
			  register struct Pointer *p;
			  /* No need to push these */
#ifdef DEBUG
			  printf( "Reducing if\n" );
#endif
			  PPUSH(stacknode);
			  FENCERED( stacknode->right );	
			  PPOP;
			  node = stacknode;
			  if( node->right.tag != BOOL ) {
				errorstart();
				errorstr( "guard not bool" );
				errorend();
				quit();
			  };
			  code = INDIRECT( &node->left );
			  /* Get branch we want, free up other. */
			  if( GETBOOL(&node->right) ) {
				p = &code->left;
				CALLIFPOINTER(&code->right,FreeMultiple);
			  }
			  else {
				p = &code->right;
				CALLIFPOINTER(&code->left,FreeMultiple);
			  };
#ifdef DEBUG
			  printf( "If choses %s\n", 
				GETBOOL(&node->right)?"then":"else" );
#endif
			  UNREL(p);
			  node->tag = IDENTITY;
			  node->left = *p;
			  FRCELL( code );
			  goto doInode;
			};
		case APPLICATION:
#ifdef DEBUG
			printf( "Reducing application\n" );
#endif
			PUSH( node );
			if( node->left.tag == ABSOLUTE ) {
				stacknode = INDIRECTABS( &node->left );
				goto mainloop;
			}
			else if( node->left.tag == RELATIVE ) {
				stacknode = INDIRECTREL( &node->left );
				goto mainloop;
			};
			errorstart();
			errorstr( "lhs of application is not function" );
			errorend();
			quit();
		case LAMBDA:
		case HEADLAMBDA:
			{ register struct EvalNode *app, *head;
			  register struct EvalNode *lam2;
			  register struct Pointer *formal;
			  /* Due to careful arrangement of this routine, do not
			   * need to PPUSH any of these. */
#ifdef DEBUG
			  printf( "Beta reduction\n" );
#endif
			  /* Any arguments present? */
			  if( sp == spfence ) {
				struct Pointer s;
#ifdef DEBUG
				printf( "Returning a function..\n");
#endif
				/* Return a pointer to ourselves ... cf CONS */
				s.tag = ABSOLUTE;
				s.data = (unsigned) node;
				return( s );
			  };
			  if( node->refcount == 1 ) {
				/* Have this lambda all to ourselves.
				 * Get second half of lambda node */
				lam2 = INDIRECT( &node->right );
				/* Get the head of the body */
				head = lam2+1;
			  	/* Get the first formal posn. */
			  	if( lam2->left.tag != NIL )
					formal = INDIRECTFORM( &lam2->left );
			  	else
					formal = NULL;
				/* Can reclaim the lambda cells */
				FRCELL( node );
				FRCELL( lam2 );
				/* Also reclaim patch list */
				CALLIFPOINTER(&lam2->right, FreeMultiple);
			  }
			  else {
				register int len;
				/* Make a new body. Don't copy lambdas or
				 * patch list. Don't need to up refs if
				 * this is the head lambda. */
				len = GETINT(&node->left) + 1;
				PPUSH(stacknode);
				head = GetBlock( len-1 );
				PPOP;
				node = stacknode;
				/* Get second half of lambda node */
				lam2 = INDIRECT( &node->right );
				if( node->tag == HEADLAMBDA ) {
					register struct EvalNode *src;
					/* Tempry use of app */
					app = head; src = lam2+1;
#ifdef DEBUG
					printf( "Copying body\n" );
#endif
					while( --len )
						*app++ = *src++;
				}
				else {	register struct EvalNode *src;
					/* Tempry use of app to copy &
					 * look for refs. */
					app = head; src = lam2+1;
#ifdef DEBUG
					printf( "Copying body & looking for references\n" );
#endif
					while( --len ) {
						ADDREF(&src->left);
						ADDREF(&src->right);
						*app++ = *src++;
					};
				};
				/* Reduce count on lambda */
				--node->refcount;
				/* Reuse len ... now holds offset of new block
				 * from old */
				len = (int) head - (int) (lam2+1);
				/* Follow patch list, UNRELing pointers we have
				 * separated. Reuse app again */
				app = NEXTREL(&lam2->right);
				while( app != NULL ) {
#ifdef DEBUG
					printf( "Patching pointer\n" );
#endif
					formal = POINTOFF(INDIRECTFORM(&app->left),len);
					formal->tag = ABSOLUTE;
					formal->data = (unsigned)
						formal->data + (int) formal
						- len;
					/* Made a copy */
					ADDREF(formal);
					app = NEXTREL(&app->right);
				};
			  	/* Get the first formal posn. */
			  	if( lam2->left.tag != NIL )
					formal = POINTOFF( 
						INDIRECTFORM( &lam2->left ),
						len);
			  	else
					formal = NULL;
			  };
			  /* Get the application node */
			  app = POP;
			  /* Copy argument into body */
			  if( formal != NULL ) {
				register struct Pointer *arg;
				arg = &app->right;
				UNREL(arg);
				while( formal->tag != NIL ) {
					register struct Pointer *m;
					m = POINTOFF( formal, GETINT(formal) );
					*formal = *arg;
					ADDREF(arg);
					formal = m;
				};
				/* No addref here to compensate for freeing 
				 * app. node */
				*formal = *arg;
			  };
			  /* Overwrite application node with head of body.
			   * May require rejigging of rel. pointers */
			  UNREL( &head->left );
			  UNREL( &head->right );
			  /* Set head count */
			  head->refcount = app->refcount;
			  /* Overwrite app with head */
			  *app = *head;
			  /* Free head */
			  FRCELL( head );
			  /* And start reducing again from head. */
			  stacknode = app;
			  goto mainloop;
			};
		default:
#ifdef DEBUG
			if( node->tag == FREE )
				printf( "Hit free node\n" );
#endif
			errorstart();
			errorstr( "broken in Reduce" );
			errorend();
			quit();
	};
		
};

/* Prettyprint a list. */
static void PrettyList( fd, stackn )
FILE *fd;
struct EvalNode *stackn;
{	register struct EvalNode *n;
	/* Need to note stackn */
	PPUSH(stackn);
	REDPOINT( stackn->left );
	n = stackn;
	if( n->left.tag == CHAR )
		/* Try to print it as a string */
		for(;;) {
			fprintf( fd, "%c", GETCHAR(&n->left) );
			REDPOINT( stackn->right );
			fflush( fd );
			n = stackn;
			if( n->right.tag == NIL )
				break;	/* End of string */
			if( !ISPOINTER(&n->right) ) {
				errorstart();
				errorstr( "broken in PrettyList" );
				errorend();
				quit();
			};
			/* Move into new node */
			stackn = INDIRECT( &n->right );
			REDPOINT( stackn->left );
			n = stackn;
			/* Make sure it's still a list of char */
			if( n->left.tag != CHAR ) {
				/* Arrg! give up. */
				fprintf( fd, "++" );
				PPOP;	/* Pop stackn */
				PrettyList( fd, n );
				return;
			};
		}
	else {
		fprintf( fd, "[" );
		PrettyLeft( fd, n );
		REDPOINT( stackn->right );
		n = stackn;
		while( n->right.tag != NIL ) {
			fprintf( fd, "," );
			stackn = n = INDIRECT( &n->right );
			PrettyLeft( fd, n );
			REDPOINT( stackn->right );
			n = stackn;
		};
		fprintf( fd, "]" );
		fflush( fd );
	};
	PPOP;	/* Pop stackn */
};

/* Prettyprint a graph, starting from a pointer. Only use for pointers
 * which we know will not move! (eg. ScriptRoot). The only call
 * to this routine is from EvaluateScript. */
static void PrettyPointer( fd, p )
FILE *fd;
register struct Pointer *p;
{	/* No need to push p (and couldn't anyway) */
	REDPOINT( *p );
	switch( p->tag ) {
		case NUM:	fprintf( fd, "%d", GETINT(p) );
				fflush( fd );
				break;
		case BOOL:	fprintf( fd, "%s", GETBOOL(p)?"true":"false" );
				fflush( fd );
				break;
		case CHAR:	fprintf( fd, "'%c'", GETCHAR(p) );
				fflush( fd );
				break;
		case NIL:	fprintf( fd, "[]" );
				fflush( fd );
				break;
		case RELATIVE:	
		case ABSOLUTE:	{ struct EvalNode *n;
				  /* Don't need to PPUSH this one .. */
				  n = INDIRECT( p );
				  if( n->tag == CONS )
					PrettyList( fd, n );
				  else {
					fprintf( fd, "<function>" );
				  };
				};
				break;
		default:	errorstart();
				errorstr( "broken in PrettyPointer" );
				errorend();
				quit();
	};
};

/* Prettyprint a graph, starting from the LHS of a node. Node may move. */
static void PrettyLeft( fd, stacknode )
FILE *fd;
struct EvalNode *stacknode;
{	register struct Pointer *p;
	/* Declare node */
	PPUSH(stacknode);
	REDPOINT( stacknode->left );
	PPOP;
	p = &stacknode->left;			/* Get result */
	switch( p->tag ) {
		case NUM:	fprintf( fd, "%d", GETINT(p) );
				fflush( fd );
				break;
		case BOOL:	fprintf( fd, "%s", GETBOOL(p)?"true":"false" );
				fflush( fd );
				break;
		case CHAR:	fprintf( fd, "'%c'", GETCHAR(p) );
				fflush( fd );
				break;
		case NIL:	fprintf( fd, "[]" );
				fflush( fd );
				break;
		case RELATIVE:	{ register struct EvalNode *n;
				  /* Don't need to PPUSH this one .. */
				  n = INDIRECTREL(p);
				  if( n->tag == CONS )
					PrettyList( fd, n );
				  else 
					fprintf( fd, "<function>" );
				};
				break;
		case ABSOLUTE:	{ register struct EvalNode *n;
				  /* Don't need to PPUSH this one .. */
				  n = INDIRECTABS(p);
				  if( n->tag == CONS )
					PrettyList( fd, n );
				  else 
					fprintf( fd, "<function>" );
				};
				break;
		default:	errorstart();
				errorstr( "broken in PrettyLeft" );
				errorend();
				quit();
	};
};

/* Prettyprint a graph, starting from the RHS of a node. Node may move. */
static void PrettyRight( fd, stacknode )
FILE *fd;
struct EvalNode *stacknode;
{	register struct Pointer *p;
	/* Declare node */
	PPUSH( stacknode );
	REDPOINT( stacknode->right );
	PPOP;
	p = &stacknode->right;			/* Get result.May have moved */
	switch( p->tag ) {
		case NUM:	fprintf( fd, "%d", GETINT(p) );
				fflush( fd );
				break;
		case BOOL:	fprintf( fd, "%s", GETBOOL(p)?"true":"false" );
				fflush( fd );
				break;
		case CHAR:	fprintf( fd, "'%c'", GETCHAR(p) );
				fflush( fd );
				break;
		case NIL:	fprintf( fd, "[]" );
				fflush( fd );
				break;
		case RELATIVE:	{ register struct EvalNode *n;
				  /* Don't need to PPUSH this one .. */
				  n = INDIRECTREL(p);
				  if( n->tag == CONS )
					PrettyList( fd, n );
				  else 
					fprintf( fd, "<function>" );
				};
				break;
		case ABSOLUTE:	{ register struct EvalNode *n;
				  /* Don't need to PPUSH this one .. */
				  n = INDIRECTABS(p);
				  if( n->tag == CONS )
					PrettyList( fd, n );
				  else 
					fprintf( fd, "<function>" );
				};
				break;
		default:	errorstart();
				errorstr( "broken in PrettyRight" );
				errorend();
				quit();
	};
};

/* Evaluate graph.
 * Exported. */
void EvaluateScript()
{	/* Do the graph! */
	PrettyPointer( stdout, &ScriptRoot->details.func.code );
	printf( "\n" );
};
