/* Asp applicative language
 * J.Cupitt, November 86
 * Memory allocator. Handles the two heaps and performs compaction. */

#include "asphdr.h"			/* Get main header */
#include "error.h"			/* Get error handler */
#include "evaltypes.h"			/* Get evaluator types */
#include "lextypes.h"			/* Needed by symboltypes */
#include "parsetypes.h"			/* Needed by symboltypes */
#include "symboltypes.h"		/* Get more types! */
#include "evaluate.h"			/* Need to be able to scan the
					 * evaluator stack for compaction */
#include "main.h"			/* Need to see command line args */
#include "declare.h"			/* Have to be able to zap ScriptRoot */
#include "pointermacros.h"		/* Need to be able to follow pointers
					 * in compaction */

/* Parse heap management is very simple ... */
static int *ParseHeap;			/* Points to base of parse tree heap */
static int *ParseTop;			/* Points to next free element */
static int *ParseEnd;			/* Points just beyond end */

/* An evaluation heap */
struct EvalHeap {
	struct EvalNode *top;		/* Points just beyond last used */
	struct EvalNode *end;		/* Points just beyond end. Used
					 * to check for overflow */
	struct EvalNode *heap;		/* The evaluation heap */
};

struct EvalHeap *CurrentHeap;		/* The heap we are allocating from */
static struct EvalHeap *BackgroundHeap;	/* The heap we will compact into */

int NumberCompacts = 0;			/* Number we have done */

struct EvalNode *GetBlock();		/* Need to be able to forward ref */

/* Allocate space from the parse tree heap, in sizeof(char) units. Align to
 * sizeof(int) units. Exported */
char *allocate( n )
int n;
{	register int *t, *s;
	t = ParseTop;
	s = ParseTop += 1 + ((int) (n/sizeof(int)));
	if( s > ParseEnd ) {
		errorstart();
		errorln( "parse tree heap full" );
		errorend();
		quit();
	};
	return( (char *) t );
};

/* Save a string to the parse heap.
 * Exported. */
char *strsave( str )
char *str;
{	char *t;
	t = allocate( strlen( str ) + 1 );
	return( (char *) strcpy( t, str ) );
};

/* Initailise the foreground evaluation heap. Called during compaction as well
 * as during startup. */
static void ClearEvalHeap()
{	register struct EvalNode *p;
	register struct EvalHeap *hp;
	register int n;
	hp = CurrentHeap;
	hp->top = hp->heap;				/* Heap empty */
	hp->end = hp->heap + EvalHeapSize;		/* Note end of heap. */
	p = hp->heap;					/* Set up for loop */
	n = hp->end - p + 1;
	while( --n )				/* Clear! */
		p++->tag = FREE;
};

/* Little support error */
static void memerr()
{	errorstart();
	errorstr( "unable to allocate space for heap" );
	errorend();
	quit();
};

/* Allocate space for our heaps and initialise them.
 * This function is exported */
void InitMemory()
{	ParseHeap = (int *) 
		malloc( ParseHeapSize * sizeof(int) );
	if( ParseHeap == NULL )
		memerr();
	ParseTop = ParseHeap;		/* Set top */
	ParseEnd = ParseHeap + ParseHeapSize;
	CurrentHeap = (struct EvalHeap *)
		malloc( sizeof(struct EvalHeap) );
	if( CurrentHeap == NULL )
		memerr();
	CurrentHeap->heap = (struct EvalNode *) 
		malloc( EvalHeapSize * sizeof(struct EvalNode) );
	if( CurrentHeap->heap == NULL )
		memerr();
	BackgroundHeap = (struct EvalHeap *)
		malloc( sizeof(struct EvalHeap) );
	if( BackgroundHeap == NULL )
		memerr();
	BackgroundHeap->heap = (struct EvalNode *) 
		malloc( EvalHeapSize * sizeof(struct EvalNode) );
	if( BackgroundHeap->heap == NULL )
		memerr();
	if( Counting )
		printf( "# Running with two heaps of %d nodes\n", 
			EvalHeapSize );
	ClearEvalHeap();		/* Init the forground heap */
};

/* Get a cell from the evaluation heap
 * Exported */
struct EvalNode *GetCell()
{	/* Only called during compilation and compaction. Don't bother
	 * with a DEBUG. */
	register struct EvalHeap *heap;
	heap = CurrentHeap;
	/* Allocate space from the continious section above
	 * top. Have to move top to reflect this. */
	if( heap->top == heap->end ) {
		/* Note! This really is an error. Because of calling context
		 * we know the heap is all used up. */
		errorstart();
		errorstr( "evaluation heap overflow" );	
		errorend();
		quit();
	};
	return( heap->top++ );	/* Move top, return the old one. */
};

/* There used to be a function to free a cell here -- it would mark the cell
 * as FREE and link it into a free-list hanging off the current EvalHeap. 
 * In fact we never looked at the free list, so I deleted it to save time. 
 * To free a cell now, one simply decrements the reference count and changes 
 * EvalNode.tag to FREE if it becomes zero. The compactor picks up FREE cells 
 * next time compaction is initiated. */

/* Find the length of a function. Include lambdas and the patch list. */
static int FuncLen( lam )
register struct EvalNode *lam;
{	register int n;
	n = 2 + GETINT(&lam->left);		/* Lambdas + stuff in ext */
	lam = NEXTREL( &INDIRECT(&lam->right)->right );
	while( lam != NULL ) {
		++n;
		lam = NEXTREL(&lam->right);
	};
	return( n );
};

/* Copy a lambda-block. Used in compaction. Include the header in the copy 
 * (unlike the copy in evaluate.c). While we are at it, note the relocation 
 * address in the old copy and mark all of the body as FREE. Passed pointers 
 * to the source and destination. */
static void CopyBlock( dest, lam, extent )
register struct EvalNode *dest, *lam;
register int extent;
{	register struct EvalNode *lam2;		/* Second lambda block */
	lam2 = INDIRECT( &lam->right );		/* Find second lambda block */
	*dest = *lam;				/* Copy first lambda across */
	lam->tag = REFERENCE;			/* Note relocation address */
	lam->left.tag = ABSOLUTE;
	lam->left.data = (unsigned) dest;
	dest->right.tag = ABSOLUTE;		/* Fixup pointer to second */
	dest++->right.data = (unsigned) (dest + 1);
	while( --extent ) {	
		*dest++ = *lam2;		/* Copy the body */
		lam2++->tag = FREE;		/* Remove from old heap */
	};
};

/* Compact the evaluation heap. Complicated! In order, we
 * 	- Copy untweaked lambda blocks into the new heap, allocating
 *	them space from the bottom. Mark the old lambda blocks as FREE, so
 *	we don't see them again. Note the address we have relocated the block
 *	to in the old lambda node (turn it into REFERENCE).
 *	- Copy across all the other bits of graph, allocating space from
 *	the top of the new heap. Turn the old node into a REFERENCE node
 *	holding the relocation address.
 *	- Scan the whole of the new heap looking for absolute pointers. These
 *	still point into the old heap, and should all point at REFERENCE nodes.
 * 	Redirect these pointers into the correct address in the new heap.
 *	- Scan the application node stack fixing up all those pointers. Again,
 *	should all point to REFERENCE nodes.
 *	- Scan the PointerStack, fixing up all those C pointers. Again, should
 *	all point to REFERENCE nodes.
 *	- Return, hoping we didn't miss anything ... */
static void CompactHeap()
{	register struct EvalNode *s, *t;	/* Scan heaps with these */
	struct EvalHeap *fore, *back;		/* Hold points on stack */
	register int n;				/* Loop counts in here */
#ifdef DEBUG
	printf( "Starting compaction\n" );
#endif
	/* Swap the two heaps over ... */
	back = CurrentHeap;
	fore = BackgroundHeap;
	CurrentHeap = fore;
	BackgroundHeap = back;
	/* We compact from back into fore. Clear our new heap */
	ClearEvalHeap();
#ifdef DEBUG
	printf( "Copying graph\n" );
#endif
	/* Scan the old (now background) heap looking for lambda-blocks or 
	 * otherwise we can copy across */
	s = back->heap;
	n = back->end - s + 1;
	while( --n ) {
		if( s->tag == LAMBDA || s->tag == HEADLAMBDA ) {
			/* Found one to copy across. Have to continue scanning 
			 * from where we are, in case the lambda node has been 
			 * moved away from the body. */
			int n;
			n = FuncLen( s );
			CopyBlock( GetBlock( n ), s, n );
		}
		else if( s->tag != FREE && s->tag != REFERENCE ) {
			/* Found something else. Get rid of relative pointers,
			 * copy it into the new heap and make the old node a
			 * REFERENCE. */
			UNREL( &s->left );	
			UNREL( &s->right );
			t = GetCell();	
			*t = *s;	
			s->tag = REFERENCE;
			s->left.tag = ABSOLUTE;
			s->left.data = (unsigned) t;			
		};
		++s;
	};
#ifdef DEBUG
	printf( "Fixing up absolute pointers\n" );
#endif
	/* Copied the whole graph. Now scan our new heap fixing up absolute 
	 * pointers. They *should* all point to REFERENCE nodes. Only need 
	 * scan nodes below top. Could use ForAllCell, but we want the couple
	 * of microsecs we save by repeating ourselves. */
	s = fore->heap;			/* Set up for loop again */
	n = fore->top - s + 1;
	while( --n ) {
		switch( s->tag ) {
			/* Match all unary cells */
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
				if( s->left.tag == ABSOLUTE ) {
					/* Found a pointer. 
					 * Find the REFERENCE node */
					t = INDIRECTABS(&s->left);
					if( t->tag != REFERENCE ) {
						/* Something funny! */
						errorstart();
						errorstr( "broken in CompactHeap" );
						errorend();
						quit();
					};
					/* Relocate! */
					s->left.data = (unsigned) 
						INDIRECTABS(&t->left);
				};
				break;
			/* Match all binary cells */
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
			case APPLICATION:	
			case IF:	
				if( s->left.tag == ABSOLUTE ) {
					/* Found a pointer. 
					 * Find the REFERENCE node */
					t = INDIRECTABS(&s->left);
					if( t->tag != REFERENCE ) { 
						errorstart();
						errorstr( "broken in CompactHeap" );
						errorend();
						quit();
					};
					/* Relocate! */
					s->left.data = (unsigned) 
						INDIRECTABS(&t->left);
				};
				if( s->right.tag == ABSOLUTE ) {
					/* Found a pointer. 
					 * Find the REFERENCE node */
					t = INDIRECTABS(&s->right);
					if( t->tag != REFERENCE ) {
						errorstart();
						errorstr( "broken in CompactHeap" );
						errorend();
						quit();
					};
					/* Relocate! */
					s->right.data = (unsigned) 
						INDIRECTABS(&t->left);
				};
				break;
			case LAMBDA:
			case HEADLAMBDA:
				/* Funny graph format - skip over this next 
				 * node. As the heap has just been compacted,
				 * we know this is ok. Have to decrement our 
				 * loop index to compensate. */
				++s; --n; 
				break;
			case STREAM:
				/* No pointers in here. Ignore. */
				break;
			default:		
				errorstart();
				errorstr( "broken in CompactHeap" );
				errorend();
				quit();
		};
		++s;
	};
#ifdef DEBUG
	printf( "Fixing up evaluation stack\n" );
#endif
	/* Have to scan the stack of pointers to application nodes, 
	 * re-jigging them. */
	n = sp;
	while( n-- ) {
		if( Stack[n]->tag != REFERENCE ) {
			errorstart();
			errorstr( "broken in CompactHeap (eval stack)" );
			errorend();
			quit();
		};
		Stack[n] = INDIRECTABS(&Stack[n]->left);
	};
#ifdef DEBUG
	printf( "Fixing up C pointers\n" );
#endif
	/* Scan stack of C pointers. May have PPUSHed the same local twice - 
	 * in which case on the 2nd zap it will no longer be pointing at
	 * a reference node. Hence we only zap pointers still at REFERENCE. */
	n = psp;
	while( n-- ) {
		register struct EvalNode *p;
		p = *(PointerStack[n]);		/* Get contents of C pointer */
		if( p->tag == REFERENCE ) 
			*(PointerStack[n]) = INDIRECTABS(&p->left);
	};
#ifdef DEBUG
	printf( "Patching root pointer\n" );
#endif
	/* We also have to patch ScriptRoot. The first test is probably
	 * redundant, but we might as well. */
	if( ISPOINTER(&ScriptRoot->details.func.code) ) {
		register struct EvalNode *p;
		p = INDIRECT(&ScriptRoot->details.func.code);
		if( p->tag != REFERENCE ) {
			errorstart();
			errorstr( "broken in CompactHeap (ScriptRoot)" );
			errorend();
			quit();
		};
		ScriptRoot->details.func.code.data = (unsigned)
			INDIRECTABS(&p->left);
	};
	++NumberCompacts;	/* Note compaction */
#ifdef DEBUG
	printf( "Heap compacted. Resuming execution.\n" );
#endif
};

/* Grab a continuous group of cells from the heap to hold a function body. This
 * is the main allocation thing for the evaluator.
 * Exported */
struct EvalNode *GetBlock( n )
int n;
{	register struct EvalNode *t;
	register struct EvalNode *s;
	register struct EvalHeap *hp;
#ifdef DEBUG
	printf( "Allocating %d cells for a function body\n", n );
#endif
	hp = CurrentHeap;
	t = hp->top;			/* Note base of area we allocate */
	s = hp->top += n;		/* Move top, note new top in s */
	if( s > hp->end ) {		/* Check for overflow */
		/* Run out of continous space. 
		 * Call CompactHeap to copy and relocate stuff onto
		 * the background heap & stick that in the foreground. Then see
		 * if we have enough space. */
		CompactHeap();
		hp = CurrentHeap;	/* Will have changed */
		t = hp->top;		/* Note base of area we allocate */
		s = hp->top += n;	/* Move top, note new top in s */
		if( s > hp->end ) {	/* Check for overflow */
			errorstart();
			errorstr( "out of evaluation heap space" );
			errorend();
			quit();
		};
	};
	return( t );
};

/* Return the next cell that will be allocated. Used by compile.c to
 * find the lengths of functions.
 * Exported */
struct EvalNode *GetTop()
{	return( CurrentHeap->top );
};

/* Scan a range of cells, applying a void function to every Pointer.
 * Exported */
void ForAllCell( from, to, fn )
register struct EvalNode *from, *to;
register void (*fn)();
{	for( ; from < to; ++from )
		switch( from->tag ) {
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
				fn( &from->left );
				break;
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
				fn( &from->left );
				fn( &from->right );
				break;
			case STREAM:
			case HEADLAMBDA:
			case LAMBDA:
				break;
			default:
				errorstart();
				errorstr( "broken in ForAllCell" );
				errorend();
				quit();
		};
};

/* Scan the forground heap, applying a void function to every cell.
 * Exported */
void ForHeap( fn )
void (*fn)();
{	ForAllCell( CurrentHeap->heap, CurrentHeap->top, fn );
};

/* Find the number of cells in use. Used in debugging output.
 * Exported. */
int UsedSpace()
{	register struct EvalNode *p;	/* Count through heap */
	register int c;			/* Count free cells */
	register int n;			/* Loop index */
	register struct EvalHeap *hp;
	hp = CurrentHeap;
	p = hp->heap;
	n = hp->end - p + 1;
	c = 0;
	while( --n )
		if( p++->tag == FREE )
			++c;
	return( EvalHeapSize - c );
};

