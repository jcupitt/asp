/* Asp applicative language
 * J. Cupitt, November 86
 * List builders etc. */

#include "asphdr.h"			/* Get global header */
#include "lextypes.h"			/* Needed by symboltypes */
#include "evaltypes.h"			/* Needed by symboltypes */
#include "parsetypes.h"			/* Needed by symboltypes */
#include "symboltypes.h"		/* Get types we manipulate */
#include "memory.h"			/* Need memory allocation */
#include "list.h"			/* Need hd, tl etc. */

/* A cons for SymbolLists */
struct SymbolList *scons( a, x )
struct Symbol *a;
struct SymbolList *x;
{	struct SymbolList *t;
	t = GetVec(struct SymbolList);
	t->this = a;
	t->next = x;
	return( t );
};

/* A cons for PatchLists */
struct PatchList *pcons( a, x )
int *a;
struct PatchList *x;
{	struct PatchList *t;
	t = GetVec(struct PatchList);
	t->this = a;
	t->next = x;
	return( t );
};

/* A cons for ExpressionLists */
struct ExpressionList *econs( a, x )
struct ExpressionNode *a;
struct ExpressionList *x;
{	struct ExpressionList *t;
	t = GetVec(struct ExpressionList);
	t->this = a;
	t->next = x;
	return( t );
};

/* cat two plists together */
struct PatchList *pcat( a, b )
struct PatchList *a, *b;
{	register struct PatchList *n;
	if( a == NULL )
		/* Easy case */
		return( b );
	/* Find the end of a */
	n = a;
	while( tl(n) != NULL )
		n = tl(n);
	/* Link on b */
	n->next = b;
	/* Return a */
	return( a );
};

/* Scan an PatchList patching pointers. See AddReference and ResolveNames. */
void PatchRefs( plist, t )
struct PatchList *plist;
int t;
{	if( plist == NULL )
		return;
	*plist->this = t;
	PatchRefs( tl(plist), t );
};

