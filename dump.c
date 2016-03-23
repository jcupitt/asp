/* Asp applicative langauge
 * J.Cupitt, November 86
 * Parse tree dumper */

#include "asphdr.h"			/* Get main header */
#include "lextypes.h"			/* Needed by symboltypes */
#include "evaltypes.h"			/* Needed by symboltypes */
#include "parsetypes.h"			/* Needed by symboltypes */
#include "symboltypes.h"		/* Get types we manipulate */
#include "error.h"			/* Need error handling */
#include "list.h"			/* Need list handlers */
#include "pointermacros.h"		/* Need to walk graph */

/* Need to declare this in advance ... */
void DumpNode();

/* Some spaces */
static void 
spc( n )
int n;
{	while( n-- )
		printf("|");
}

/* Decode a SymbolTag */
static char *
DecodeSTag( tag )
enum SymbolTags tag;
{	switch( tag ) {
		case PARAMETER:		return( "PARAM" );
		case FUNCTION:		return( "FUNC" );
		case UNRESOLVED:	return( "UNRES" );
		case TOPFUNC:		return( "TOPFUNC" );
		case DEAD:		printf( "Hit a DEAD!\n" );
		default:		errorstart();
					errorstr( "broken in DecodeSTag" );
					errorend();
					quit();
	}
}

/* Decode a Bool */
static char *
DecodeBool( bool )
enum Boolean bool;
{	switch( bool ) {
		case TRUE:	return( "TRUE" );
		case FALSE:	return( "FALSE" );
		default:	errorstart();
				errorstr( "broken in DecodeBool" );
				errorend();
				quit();
	}
}

/* Decode a binary operator */
static char *
DecodeBiop( tag )
char tag;
{	switch( tag ) {
		case CONS:		return( "Cons" );
		case BINARYPLUS:	return( "Binary plus" );
		case BINARYMINUS:	return( "Binary minus" );
		case TIMES:		return( "Times" );
		case DIVIDE:		return( "Divide" );
		case LESS:		return( "Less than" );
		case MORE:		return( "Greater than" );
		case EQUAL:		return( "Equal to" );
		case MOREEQUAL:		return( "Greater than or equal to" );
		case LESSEQUAL:		return( "Less than or equal to" );
		case NOTEQUAL:		return( "Not equal to" );
		case AND:		return( "And" );
		case OR:		return( "Or" );
		case APPLICATION:	return( "Function application" );
		case COMP:		return( "Function composition" );
		case WRITE:		return( "Write" );
		default:		errorstart();
					errorstr( "broken in DecodeBiop" );
					errorend();
					quit();
	}
}

/* Decode a unary operator */
static char *
DecodeUop( tag )
char tag;
{	switch( tag ) {
		case DECODE:		return( "Decode" );
		case CODE:		return( "Code" );
		case ERROR:		return( "Error" );
		case IDENTITY:		return( "Identity" );
		case UNARYMINUS:	return( "Unary minus" );
		case NOT:		return( "Not");
		case HD:		return( "Hd" );
		case TL:		return( "Tl" );
		case READ:		return( "Read" );
		default:		errorstart();
					errorstr( "broken in DecodeUop" );
					errorend();
					quit();
	}
}

/* Dump a const. */
static void 
DumpConst( indent, c )
int indent;
struct ConstVal *c;
{	switch( c->tag ) {
		case NUM:	printf( "Num %d\n", c->details.intconst );
				break;
		case BOOL:	printf( "Bool %s\n", 
					c->details.boolconst?"true":"false" );
				break;
		case CHAR:	printf( "Char '%c'\n", c->details.charconst );
				break;
		case NIL:	printf( "Nil\n" );
				break;
		default:	errorstart();
				errorstr( "broken in DumpConst" );
				errorend();
				quit();
	}
}

/* Dump a symbol. */
static void
DumpSymbol( sym )
struct Symbol *sym;
{	printf( "symbol: %s, tag: %s, from: %d:%s, used: %s",
		sym->sname,
		DecodeSTag( sym->tag ),
		sym->declaredat,
		sym->fileat,
		DecodeBool( sym->used ) );
	if( ISFUNC(sym) )
		printf( ", args: %d\n", sym->details.func.nargs );
	else if( ISPARM(sym) )
		printf( ", declaredin: %s\n", 
			sym->details.param.declaredin->sname );
	else
		printf( "\n" );
}
					
/* Dump expression.
 * Exported */
void 
DumpExpression( indent, expr )
int indent;
struct ExpressionNode *expr;
{	spc( indent );
	switch( expr->tag ) {
		case IDENTITY:
		case ERROR:
		case READ:
		case CODE:
		case DECODE:
		case UNARYMINUS:
		case NOT:
		case HD:
		case TL:		
			printf( "%s\n", DecodeUop( expr->tag ) );
			DumpExpression( indent+1, expr->details.thisop );
			break;
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
		case COMP:
			printf( "%s\n", DecodeBiop( expr->tag ) );
			DumpExpression( indent+1, expr->details.binop.leftop );
			DumpExpression( indent+1, expr->details.binop.rightop );
			break;
		case VARREF:		
			DumpSymbol( expr->details.ident );
			break;
		case IF:		
			printf( "If\n" );
			spc( indent );
			printf( "Condition\n" );
			DumpExpression( indent+1, 
				expr->details.ifnode.condition );
			spc( indent );
			printf( "Then\n" );
			DumpExpression( indent+1, 
				expr->details.ifnode.thenpart );
			spc( indent );
			printf( "Else\n" );
			DumpExpression( indent+1, 
				expr->details.ifnode.elsepart );
			break;
		case GEN1:		
			printf( "GEN1; from\n" );
			DumpExpression( indent+1, expr->details.gennode.gen1 );
			break;
		case GEN2:		
			printf( "GEN2; from\n" );
			DumpExpression( indent+1, expr->details.gennode.gen1 );
			spc( indent );
			printf( "to\n" );
			DumpExpression( indent+1, expr->details.gennode.gen2 );
			break;
		case GEN3:		
			printf( "GEN3; from\n" );
			DumpExpression( indent+1, expr->details.gennode.gen1 );
			spc( indent );
			printf( "steps of\n" );
			DumpExpression( indent+1, expr->details.gennode.gen2 );
			break;
		case GEN4:		
			printf( "GEN4; from\n" );
			DumpExpression( indent+1, expr->details.gennode.gen1 );
			spc( indent );
			printf( "steps of\n" );
			DumpExpression( indent+1, expr->details.gennode.gen2 );
			spc( indent );
			printf( "to\n" );
			DumpExpression( indent+1, expr->details.gennode.gen3 );
			break;
		case CONSTANT:		
			DumpConst( indent, expr->details.cons );
			break;
		default:		
			errorstart();
			errorstr("broken #2 in DumpExpression");
			errorend();
			quit();
	}
}

/* Dump a struct Pointer *.
 * Exported. */
void 
DumpGraph( indent, p )
int indent;
struct Pointer *p;
{	spc( indent );
	printf( "POINTER at %x; tag = ", (int) p );
	switch( p->tag ) {
		case NUM:	
			printf( "NUM %d\n", GETINT(p) );
			break;
		case BOOL:	
			printf( "BOOL %s\n", GETBOOL(p)?"true":"false" );
			break;
		case CHAR:	
			printf( "CHAR '%c'\n", GETCHAR(p) );
			break;
		case NIL:	
			printf( "NIL\n" );
			break;
		case EXTENT:	
			printf( "EXTENT %d\n", GETINT(p) );
			break;
		case ABSOLUTE:	
			printf( "ABSOLUTE to:\n" );
			DumpNode( indent+1, INDIRECT(p) );
			break;
		case RELATIVE:	
			printf( "RELATIVE to:\n" );
			DumpNode( indent+1, INDIRECT(p) );
			break;
		case SOURCE:	
			printf( "SOURCE to " );
			DumpSymbol( INDIRECTSYM(p) );
			break;
		case FORMAL:	
			printf( "FORMAL to %x\n", INDIRECTFORM(p) );
			break;
		default:	
			errorstart();
			errorstr( "broken in DumpGraph" );
			errorend();
			quit();
	}
}

/* Dump an EvalNode. */
void 
DumpNode( indent, node )
int indent;
struct EvalNode *node;
{	spc( indent );
	printf( "NODE at %x; count = %d; tag = ", (int) node, node->refcount );
	switch( node->tag ) {
		case ERROR:
		case READ:
		case CODE:
		case DECODE:
		case IDENTITY:
		case UNARYMINUS:
		case NOT:
		case HD:
		case TL:		
			printf( "%s\n", DecodeUop( node->tag ) );
			DumpGraph( indent+1, &node->left );
			break;
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
		case COMP:
			printf( "%s\n", DecodeBiop( node->tag ) );
			DumpGraph( indent+1, &node->left );
			DumpGraph( indent+1, &node->right );
			break;
		case IF:		
			printf( "If\n" );
			DumpGraph( indent+1, &node->right );
			spc( indent );
			printf( "Then\n" );
			DumpGraph( indent+1, &INDIRECT(&node->left)->left );
			spc( indent );
			printf( "Else\n" );
			DumpGraph( indent+1, &INDIRECT(&node->left)->right );
			break;
		case GEN1:		
			printf( "GEN1; from\n" );
			DumpGraph( indent+1, &node->left );
			break;
		case GEN2:		
			printf( "GEN2; from\n" );
			DumpGraph( indent+1, &node->left );
			spc( indent );
			printf( "to\n" );
			DumpGraph( indent+1, &node->right );
			break;
		case GEN3:		
			printf( "GEN3; from\n" );
			DumpGraph( indent+1, &node->left );
			spc( indent );
			printf( "step\n" );
			DumpGraph( indent+1, &node->right );
			break;
		case GEN4:		
			printf( "GEN4; from\n" );
			DumpGraph( indent+1, &node->left );
			spc( indent );
			printf( "step\n" );
			DumpGraph( indent+1, &INDIRECT(&node->right)->left );
			spc(indent);
			printf( "to\n" );
			DumpGraph( indent+1, &INDIRECT(&node->right)->right );
			break;
		case HEADLAMBDA:
		case LAMBDA:		
		      	{	struct EvalNode *lam2;
			 	printf( "LAMBDA; extent = %d;\n", 
					GETINT(&node->left) );
				spc(indent);
				printf( "Formal list " );
				lam2 = INDIRECT( &node->right );
				if( lam2->left.tag == NIL )
					printf( "NIL; " );
				else
				printf( "%x; ", INDIRECTFORM( &lam2->left ) );
				printf( "patch list:\n" );
				DumpGraph( indent+1, &lam2->right );
				spc( indent );
				printf( "Body:\n" );
					 DumpNode( indent+1, lam2+1 );
			}
			break;
		default:		
			errorstart();
			errorstr( "broken in DumpNode" );
			errorend();
			quit();
	}
}

static int CountNode();

/* Count nodes in graph from a pointer. 
 * Exported. */
int 
CountGraph( p )
struct Pointer *p;
{	switch( p->tag ) {
		case NUM:	
		case BOOL:	
		case CHAR:	
		case NIL:	
		case SOURCE:	
		case FORMAL:	
		case EXTENT:	
			return( 0 );
		case ABSOLUTE:	
			return( CountNode( INDIRECTABS(p) ) );
		case RELATIVE:	
			return( CountNode( INDIRECTREL(p) ) );
		default:	
			errorstart();
			errorstr( "broken in CountGraph" );
			errorend();
			quit();
	}
}

/* Count an EvalNode. */
static int 
CountNode( node )
struct EvalNode *node;
{	switch( node->tag ) {
		case GEN1:
		case ERROR:
		case READ:
		case CODE:
		case DECODE:
		case IDENTITY:
		case UNARYMINUS:
		case NOT:
		case HD:
		case TL:		
			return( 1+CountGraph(  &node->left ) );
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
			return( 1+CountGraph( &node->left )+ 
				CountGraph( &node->right) );
		case HEADLAMBDA:
		case LAMBDA:		
			errorstart();
			errorstr( "CountNode can't count functions" );
			errorend();
			return(0);
		default:		
			errorstart();
			errorstr( "broken in CountNode" );
			errorend();
			quit();
	}
}

/* Dump a SymbolList */
static void 
DumpSList( list )
struct SymbolList *list;
{	if( list == NULL )
		return;
	DumpSymbol( hd(list) );
	DumpSList( tl(list) );
}

static void DumpSym();		/* Forward ref this */

/* Dump a symbol table
 * Exported */
void DumpST( table )
struct SymbolTable *table;
{	ForAllSym( table, DumpSym );
}

/* Dump a symbol */
static void DumpSym( sym )
struct Symbol *sym;
{	DumpSymbol( sym );
	if( ISFUNC(sym) ) {
		printf( "*** params of %s\n", sym->sname );
		DumpSList( sym->details.func.params );
		printf( "*** externs of %s\n", sym->sname );
		DumpSList( sym->details.func.externs );
		printf( "*** body of %s\n", sym->sname );
		DumpExpression( 2, sym->details.func.body );
		printf( "*** locals of %s\n", sym->sname );
		DumpST( sym->details.func.locals );
		printf( "*** end of %s\n", sym->sname );
	}
}
