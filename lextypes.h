/* Asp applicative language
 * J.Cupitt, November 86
 * Types for the lexical analyser */

/* Types we return */
#define NUM 	5
#define CHAR 	1
#define BOOL	2
#define NIL	3
#define STRING	4

/* Something to return */
struct ConstVal {
	char tag;
	union {
		int intconst;			/* A number !!! */
		char charconst;			/* Character constant */
		enum Boolean boolconst;		/* Bool const */
		char *stringconst;		/* Body of string const. */
	} details;
};

