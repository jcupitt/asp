	/* Parser for the Asp applicative language.
	 * J.Cupitt, November 86
	 *
	 * We build the parse tree and the symbol table for the script 
	 * in this single pass. A later stage builds the graph to be
	 * reduced from this. */

%{
#include "asphdr.h"			/* Main include file */
#include "lextypes.h"			/* Need to handle tokens */
#include "evaltypes.h"			/* Needed by parsetypes.h */
#include "symboltypes.h"		/* Types for the symbol table */
#include "parsetypes.h"			/* Types for the parse tree */
#include "symbol.h"			/* Headers for symbol table fns */
#include "io.h"				/* Lex io functions */
#include "tree.h"			/* Tree building functions */
#include "error.h"			/* Error handler function headers */

#undef input
#undef output
#undef unput
#undef yywrap
#undef yyerror

/* The function currently being parsed. A nasty hack. */
static struct Symbol *CurrentSymbol;

/* The current internal and external tables */
struct SymbolTable *CurrentTable;

/* The size of the hash tables we use. BIGTABLE is the main global name
 * table, SMALLTABLE is the size of the table attached to  */
#define SMALLTABLE 13
#define BIGTABLE 113

/* Parse a function in few stages:
 * - Make a new symbol. Add it to the old table. This symbol is the name of the
 * function we are defining
 * - Save the old symbol on a stack
 * - Make a new table, attach it to the new symbol. This table is to hold
 * the names of any parameters and local definitions for this function.
 * - As we parse the params & body of this function, new names are added to
 * this table.
 * - At the end of the function, pop the old symbol from the stack and
 * transfer any UNRESOLVED symbols from the new symbol onto the table attached
 * to the old symbol. This transfering is done by ResolveNames in symbol.c. */
%}

%start script

%token TK_IDENTIFIER TK_HD TK_TL TK_INCLUDE TK_WRITE
%token TK_LESSEQUAL TK_MOREEQUAL TK_NOTEQUAL TK_CONST
%token TK_AND TK_OR TK_UMINUS TK_UPLUS TK_CODE TK_DECODE TK_ERROR
%token TK_READ TK_DOTS 

%type <cons> '+' '-' '*' '/' ':' '[' ']' '(' ')' ',' '~' '<' '>' '=' ';' '.'
%type <cons> TK_CONST TK_IDENTIFIER 
%type <cons> TK_LESSEQUAL TK_MOREEQUAL TK_NOTEQUAL
%type <expression> expression list_elements unary_fn binary_fn 
%type <expression> list_body rhs_list

%left TK_AND TK_OR
%left '+' '-'
%left '*' '/'
%left TK_UMINUS TK_UPLUS TK_ERROR TK_CODE TK_DECODE TK_READ TK_DOTS TK_WRITE
%right TK_IDENTIFIER TK_HD TK_TL TK_CONST '[' '(' '~'
%right ':' '.'
%nonassoc '<' '>' '=' TK_LESSEQUAL TK_MOREEQUAL TK_NOTEQUAL

%%			/* Start of rules section */
			
			/* An asp script. Set up the global symbol
			 * table */
script		:	/* Nowt */
			{ /* Make the root ST */
			  CurrentSymbol = MakeRoot();
			  CurrentTable = CreateTable( BIGTABLE );
			  AddTable( CurrentSymbol, CurrentTable );
			}
			function_list
		;

function_list	:	/* Empty */
		| 	function_list function_decl
		| 	comp_directive
		;

			/* Do #include */
comp_directive	:	TK_INCLUDE TK_CONST
				{ if( $2->tag != STRING ) {
					errorstart();
					errorstr( "include must have string arg" );
					errorend();
					quit();
				  }
				  AddFile( $2->details.stringconst );
				}
		;

			/* Declare a function */
function_decl	:	TK_IDENTIFIER
				{ /* Save the old symbol */
				  PushSymbol( CurrentSymbol );
				  /* Make this name in the current table */
				  CurrentSymbol = AddName( CurrentTable, 
					$1->details.stringconst );
#ifdef DEBUG
				  printf("Adding %s as a global\n", $1->details.stringconst);
#endif
				  /* Make the local table for this function */
				  CurrentTable = CreateTable( SMALLTABLE );
				  /* Link it on */
				  AddTable( CurrentSymbol, CurrentTable );
				}
			identifier_list rhs_list
				{ /* Add the body to this function */
				  AddBody( CurrentSymbol, $4 );
#ifdef DEBUG
				  printf("Adding body to function %s\n",
					CurrentSymbol->sname);
#ifdef TREES
				  DumpExpression( 0, $4 );
#endif
#endif
				}
			local_defs
				{ struct Symbol *oldsym;
				  /* Get back old symbol */
				  oldsym = PopSymbol();
			 	  /* Having parsed local defs, copy any
				   * UNRESOLVED syms onto oldsym's table */
				  ResolveNames( oldsym->details.func.locals, 
					CurrentTable, 
					CurrentSymbol );
				  /* Restore the previous current symbol */
				  CurrentSymbol = oldsym;
				  CurrentTable = oldsym->details.func.locals;
				}
		;

			/* Parse those local defs */
local_defs	:	/* Empty */
		|	'{' function_list '}'
		;

			/* Parse a list of local identifiers. Add to locals
			 * attached to the current symbol. */
identifier_list	:	identifier_list TK_IDENTIFIER 
				{ AddParam( CurrentSymbol, 
					$2->details.stringconst );
#ifdef DEBUG
				  printf("Adding local %s\n",
					$2->details.stringconst);
#endif
				}
		|	/* Empty */ 
		;

			/* Parse a list of RHS. Build the list of IF's necessary */
rhs_list	:	'=' expression ';'
				{ $$ = $2;
				}
		|	'=' expression ',' expression ';' rhs_list
				{ $$ = BuildIf( $4, $2, $6 );
				}
		;

			/* Parse an expression. */
expression	:	'(' expression ')'
				{ $$ = $2;
				}
		|	TK_IDENTIFIER
				{ struct Symbol *s;
				  struct ExpressionNode *n;
				  /* Look up in current scope */
				  s = FindName( CurrentTable, 
					$1->details.stringconst );
				  n = BuildVarRef( s );
				  /* Add to reference list */
				  AddReference( s, n );
				  $$ = n;
				}
		|	expression expression
					%prec ':'
				{ $$ = BuildBiop( APPLICATION, $1, $2 );
				}
		|	TK_CONST
				{ $$ = BuildConst( $1 );
				}
		| 	'[' list_body ']'
				{ $$ = $2;
				}
		| 	unary_fn
		|	binary_fn
		;

			/* Various types of list body */
list_body	:	expression TK_DOTS
				{ $$ = BuildListGen1( $1 );
				}
		|	expression TK_DOTS expression
				{ $$ = BuildListGen2( $1, $3 );
				}
		|	expression ',' expression TK_DOTS
				{ $$ = BuildListGen3( $1, $3 );
				}
		|	expression ',' expression TK_DOTS expression
				{ $$ = BuildListGen4( $1, $3, $5 );
				}
		|	list_elements
		;

			/* Parse a list constant body */
list_elements	:	expression ',' list_elements
				{ $$ = BuildBiop( CONS, $1, $3 );
				}
		|	expression
				{ $$ = BuildBiop( CONS, $1, NilTree );
				}
		;

			/* Parse an infix binary function */
binary_fn	:	expression ':' expression
				{ $$ = BuildBiop( CONS, $1, $3 );
				}
		|	expression '+' expression
				{ $$ = BuildBiop( BINARYPLUS, $1, $3 );
				}
		|	expression '-' expression
				{ $$ = BuildBiop( BINARYMINUS, $1, $3 );
				}
		|	expression '*' expression
				{ $$ = BuildBiop( TIMES, $1, $3 );
				}
		|	expression '/' expression
				{ $$ = BuildBiop( DIVIDE, $1, $3 );
				}
		|	expression '<' expression
				{ $$ = BuildBiop( LESS, $1, $3 );
				}
		|	expression '>' expression
				{ $$ = BuildBiop( MORE, $1, $3 );
				}
		|	expression '=' expression
				{ $$ = BuildBiop( EQUAL, $1, $3 );
				}
		|	expression '.' expression
				{ $$ = BuildBiop( COMP, $1, $3 );
				}
		|	expression TK_LESSEQUAL expression
				{ $$ = BuildBiop( LESSEQUAL, $1, $3 );
				}
		|	expression TK_MOREEQUAL expression
				{ $$ = BuildBiop( MOREEQUAL, $1, $3 );
				}
		|	expression TK_NOTEQUAL expression
				{ $$ = BuildBiop( NOTEQUAL, $1, $3 );
				}
		|	expression TK_AND expression
				{ $$ = BuildBiop( AND, $1, $3 );
				}
		|	expression TK_OR expression
				{ $$ = BuildBiop( OR, $1, $3 );
				}
		|	expression TK_WRITE expression
				{ $$ = BuildBiop( WRITE, $1, $3 );
				}
		;

			/* Parse a prefix unary function */
unary_fn	:	TK_UMINUS expression		
				{ $$ = BuildUop( UNARYMINUS, $2 );
				}
		|	TK_UPLUS expression		
				{ $$ = $2;
				}
		|	'~' expression		
				{ $$ = BuildUop( NOT, $2 );
				}
		|	TK_HD expression
				{ $$ = BuildUop( HD, $2 );
				}
		|	TK_TL expression	
				{ $$ = BuildUop( TL, $2 );
				}
		|	TK_CODE expression
				{ $$ = BuildUop( CODE, $2 );
				}
		|	TK_DECODE expression
				{ $$ = BuildUop( DECODE, $2 );
				}
		|	TK_ERROR expression
				{ $$ = BuildUop( ERROR, $2 );
				}
		|	TK_READ expression
				{ $$ = BuildUop( READ, $2 );
				}
		;
%%

#define YY_NO_INPUT

#include "lex.yy.c"		/* Grab the lexer */

