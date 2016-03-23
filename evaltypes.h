/* Asp applicative langauge
 * J.Cupitt, November 86
 * Types for the evaluator */

/* Enum for EvalNode tags. Reuse expression #defines up to IF. 
 * See parsetypes.h */
#define LAMBDA		42
#define IDENTITY 	43
#define FREE		44
#define HEADLAMBDA	45
#define REFERENCE	46
#define STREAM		47

/* Enum for pointer tags. Reuse lexer tags (not STRING though) */
/* Pointer types */
#define ABSOLUTE	6
#define RELATIVE	7
/* And reuse of right pointer field in abstraction */
#define EXTENT		8
/* And back to the ST for x linking function bodies during compilation */
#define SOURCE		9
/* And one for formals. (Relative pointers to Pointers) */
#define FORMAL		10

/* A pointer + tag. This was originally a bitfield, with the two things
 * packed into one long (4 bits and 28 bits). This was appalingly slow
 * and so I changed it into char + int. This is why there are no types
 * for these! Change back to bitfields if space is at a premium. */
struct Pointer {
	char tag;
	int data;
};

/* An evaluation node */
struct EvalNode {
	int refcount;				/* Reference count for this node */
	char tag;				/* Tag for this node */
	struct Pointer left;			/* Left pointer */
	struct Pointer right;			/* And right pointer */
};	
