/* Asp applicative language
 * J. Cupitt, November 86
 * Types for the symbol table */

/* Types of symbol */
enum SymbolTags {
	PARAMETER, FUNCTION, UNRESOLVED, TOPFUNC, DEAD
};

/* Classifier on symbols */
#define ISFUNC(S) ((S)->tag == FUNCTION || (S)->tag == TOPFUNC)
#define ISPARM(S) ((S)->tag == PARAMETER)

/* A symbol */
struct Symbol {
	char *sname;					/* Symbol name */
	enum SymbolTags tag;				/* Tag */
	struct PatchList *refedat;			/* Referred to at */
	struct PatchList *erefedat;			/* Extern refs */
	int declaredat;					/* Line number at */
	char *fileat;					/* File declared in */
	enum Boolean used;				/* Referenced */
	union {						/* Body is one of .. */
		struct {				/* A function */
			struct SymbolTable *locals;	/* List of locals */
			struct ExpressionNode *body;	/* Body expr */
			struct Pointer code;		/* Code for function */
			int nargs;			/* No. of params */
			struct SymbolList *params;	/* Ordered params */
			struct SymbolList *externs;	/* External refs */
		} func;
		struct {
			struct Symbol *declaredin;	/* Fn declared in */
			struct Pointer *lastarg;	/* For subst. lists */
		} param;
	} details;
};

/* A list of symbols */
struct SymbolList {
	struct Symbol *this;		/* This element in list */
	struct SymbolList *next;	/* Next element */
};

/* A list of pointers .. much too clumsy to have separate versions of this
 * for every kind of patch list we make. Casts hidden in lower routines. */
struct PatchList {
	int *this;			/* This element in list */
	struct PatchList *next;		/* Next element */
};

/* A list of ExpressionNodes .. used to make lists of references to symbols */
struct ExpressionList {
	struct ExpressionNode *this;	/* This element in list */
	struct ExpressionList *next;	/* Next element */
};

/* A symbol table */
struct SymbolTable {
	int hsize;			/* Size of hash vector */
	struct SymbolList **base;	/* Hash table */
};
