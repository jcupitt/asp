/* Asp applicative language.
 * J.Cupitt, November 86
 * Declare types for the parse tree. */

/* Tags for labeling expression nodes. Reused (partly) in the evaluator. */
#define UNARYMINUS	0
#define NOT		1
#define HD		2
#define TL		3
#define CONS		4
#define BINARYPLUS	5
#define BINARYMINUS	6
#define TIMES		7
#define DIVIDE		8
#define LESS		9
#define MORE		10
#define EQUAL		11
#define MOREEQUAL	12
#define LESSEQUAL	13
#define NOTEQUAL	14
#define AND		15
#define OR		16
#define APPLICATION	17
#define IF		18
#define CONSTANT	20
#define VARREF		21
#define CODE		22
#define DECODE		23
#define ERROR		24
#define READ		25
#define GEN1		26
#define GEN2		27
#define GEN3		28
#define GEN4		29
#define WRITE		30
#define COMP		31

/* An expression node */
struct ExpressionNode {
	char tag;				/* Tag labeling this node */
	struct EvalNode *codeat;		/* What code this generated */
	union {					/* Which is one of ... */
		struct {			/* Binary op or fn appl */	
			struct ExpressionNode *leftop;
			struct ExpressionNode *rightop;
		} binop;	
		struct Symbol *ident;		/* Variable */
		struct ExpressionNode *thisop;	/* A unary operator */
		struct {			/* An IF node */
			struct ExpressionNode *condition;
			struct ExpressionNode *thenpart;
			struct ExpressionNode *elsepart;
		} ifnode;
		struct {			/* A GEN node */
			struct ExpressionNode *gen1;
			struct ExpressionNode *gen2;
			struct ExpressionNode *gen3;
		} gennode;
		struct ConstVal *cons;		/* Constant */
	} details;
};

/* The union for the parse stack */
typedef union {
	struct ExpressionNode *expression;	/* For parsing expressions */
	struct ConstVal *cons;			/* For handling tokens */
} YYSTYPE;


