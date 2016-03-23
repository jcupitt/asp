/* Asp applicative language
 * J.Cupitt, November 86
 * Tree builder */

#include "asphdr.h"			/* Get global header */
#include "lextypes.h"			/* Need to build lextypes */
#include "evaltypes.h"			/* Needed by parsetypes.h */
#include "parsetypes.h"			/* Need to build tree types */
#include "io.h"				/* Need line numbers */
#include "memory.h"			/* Need memory allocation */
#include "list.h"			/* List handling macros */

struct ExpressionNode *NilTree;		/* Nil. For building list constants */

struct ExpressionNode *BuildCharList();	/* Need to foward ref. this */

/* Build a string constant.
 * This function is exported */
struct ConstVal *BuildStringConst( string )
char *string;
{	struct ConstVal *t;
	t = GetVec(struct ConstVal);
	t->tag = STRING;
	t->details.stringconst = strsave( string );
	return( t );
}

/* Build a numeric constant
 * This function is exported */
struct ConstVal *BuildNumConst( n )
int n;
{	struct ConstVal *t;
	t = GetVec(struct ConstVal);
	t->tag = NUM;
	t->details.intconst = n;
	return( t );
}

/* Build a boolean constant
 * This function is exported */
struct ConstVal *BuildBoolConst( b )
enum Boolean b;
{	struct ConstVal *t;
	t = GetVec(struct ConstVal);
	t->tag = BOOL;
	t->details.boolconst = b;
	return( t );
}

/* Build a nil constant
 * This function is exported */
struct ConstVal *BuildNilConst()
{	struct ConstVal *t;
	t = GetVec(struct ConstVal);
	t->tag = NIL;
	return( t );
}

/* Build a character constant. Expect eg: "\n", "\t", "a"
 * This function is exported */
struct ConstVal *BuildCharConst( str )
char *str;
{	struct ConstVal *t;
	t = GetVec(struct ConstVal);
	t->tag = CHAR;
	if( *str == '\\' )
		switch( str[1] ) {
			case 'n':	t->details.charconst = '\n';
					break;
			case 'b':	t->details.charconst = '\b';
					break;
			case 't':	t->details.charconst = '\t';
					break;
			case '0':	t->details.charconst = '\0';
					break;
			default:	t->details.charconst = str[1];
					break;
		}
	else
		t->details.charconst = *str;
	return( t );
}

/* Build an ExpressionNode from a ConstVal.
 * Exported. */
struct ExpressionNode *BuildConst( c )
struct ConstVal *c;
{	if( c->tag == STRING )
		return( BuildCharList( c ) );
	else {
		struct ExpressionNode *t;
		t = GetVec(struct ExpressionNode);
		t->tag = CONSTANT;
		t->details.cons = c;
		return( t );
	}
}

/* Build a variable reference.
 * Exported */
struct ExpressionNode *BuildVarRef( sym )
struct Symbol *sym;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = VARREF;
	t->details.ident = sym;
	return( t );
}

/* Build an IF.
 * This function is exported */
struct ExpressionNode *BuildIf( exp1, exp2, exp3 )
struct ExpressionNode *exp1, *exp2, *exp3;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = IF;
	t->details.ifnode.condition = exp1;
	t->details.ifnode.thenpart = exp2;
	t->details.ifnode.elsepart = exp3;
	return( t );
};

/* Build a unary operator.
 * This function is exported */
struct ExpressionNode *BuildUop( tag, exp )
char tag;
struct ExpressionNode *exp;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = tag;
	t->details.thisop = exp;
	return( t );
};

/* Build a binary operator.
 * This function is exported */
struct ExpressionNode *BuildBiop( tag, exp1, exp2 )
char tag;
struct ExpressionNode *exp1, *exp2;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = tag;
	t->details.binop.leftop = exp1;
	t->details.binop.rightop = exp2;
	return( t );
};

/* Parcel up a string. */
static struct ExpressionNode *BuildList( str )
char *str;
{	if( *str == '\0' )
		return( NilTree );
	else 
		if( *str == '\\' )
			return( BuildBiop( CONS, BuildConst( BuildCharConst( str ) ), 
				BuildList( str+2 ) ) );
		else
			return( BuildBiop( CONS, BuildConst( BuildCharConst( str ) ), 
				BuildList( str+1 ) ) );
};

/* Turn a string into a huge list of CONS pairs. Guaranteed no empty string. */
struct ExpressionNode *BuildCharList( c )
struct ConstVal *c;
{	return( BuildList( c->details.stringconst ) );
};

/* Initialise the tree (set up NilTree!) 
 * Exported */
void InitParseTree()
{	NilTree = BuildConst( BuildNilConst() );
};

/* Make various GEN nodes. 
 * Exported */
struct ExpressionNode *BuildListGen1( exp )
struct ExpressionNode *exp;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = GEN1;
	t->details.gennode.gen1 = exp;
	return( t );
};
	
struct ExpressionNode *BuildListGen2( exp1, exp2 )
struct ExpressionNode *exp1, *exp2;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = GEN2;
	t->details.gennode.gen1 = exp1;
	t->details.gennode.gen2 = exp2;
	return( t );
};
	
struct ExpressionNode *BuildListGen3( exp1, exp2 )
struct ExpressionNode *exp1, *exp2;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = GEN3;
	t->details.gennode.gen1 = exp1;
	t->details.gennode.gen2 = exp2;
	return( t );
};	
	
struct ExpressionNode *BuildListGen4( exp1, exp2, exp3 )
struct ExpressionNode *exp1, *exp2, *exp3;
{	struct ExpressionNode *t;
	t = GetVec(struct ExpressionNode);
	t->tag = GEN4;
	t->details.gennode.gen1 = exp1;
	t->details.gennode.gen2 = exp2;
	t->details.gennode.gen3 = exp3;
	return( t );
};
	
/* Apply a function to every sub-expr in a function. Result of function
 * controlls furthur recursion. TRUE means to below this point, FALSE
 * means back out at this level. 
 * Exported */
void ApplyAll( fn, tree )
enum Boolean (*fn)();
struct ExpressionNode *tree;
{	/* Apply here */
	if( !fn( tree ) )
		/* fn tells us to back out */
		return; 
	/* Do subexprs of this subexpr */
	switch( tree->tag ) {	
		/* Unary ops */
		case CODE:
		case READ:
		case DECODE:
		case ERROR:
		case UNARYMINUS:
		case NOT:
		case HD:
		case TL:		
			ApplyAll( fn, tree->details.thisop );
			break;
		case GEN1:		
			ApplyAll( fn, tree->details.gennode.gen1 );
			break;
		/* Do binary ops */
		case AND:
		case OR:
		case CONS:	
		case BINARYPLUS:
		case WRITE:
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
		case COMP:
			ApplyAll( fn, tree->details.binop.leftop );
			ApplyAll( fn, tree->details.binop.rightop );
			break;
		case GEN2:		
		case GEN3:
			ApplyAll( fn, tree->details.gennode.gen1 );
			ApplyAll( fn, tree->details.gennode.gen2 );
			break;
		case VARREF:
			break;
		case IF:
			ApplyAll( fn, tree->details.ifnode.condition );
			ApplyAll( fn, tree->details.ifnode.thenpart );
			ApplyAll( fn, tree->details.ifnode.elsepart );
			break;
		case GEN4:
			ApplyAll( fn, tree->details.gennode.gen1 );
			ApplyAll( fn, tree->details.gennode.gen2 );
			ApplyAll( fn, tree->details.gennode.gen3 );
			break;
		case IDENTITY:
		case CONSTANT:
			break;
		default:		
			errorstart();
			errorstr( "broken in ApplyAll" );
			errorend();
			quit();
	};
};

