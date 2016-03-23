/* Asp applicative langauge
 * J.Cupitt, November 86
 * Macros for absolute/relative pointers */

#define INDIRECTABS(A) ( (struct EvalNode *) ((A)->data))
#define INDIRECTREL(A) ( NODEOFF((A),(A)->data) )
#define INDIRECTFORM(A) ( POINTOFF((A),(A)->data) )
#define INDIRECT(A) ( ((A)->tag==ABSOLUTE) ? INDIRECTABS(A) : INDIRECTREL(A) )
#define INDIRECTSYM(A) ((struct Symbol *) (A)->data)
#define GETINT(A) ((int) (A)->data)
#define GETCHAR(A) ((char) (A)->data)
#define GETBOOL(A) ((enum Boolean) (A)->data)
#define GETEXPR(A) ((struct ExpressionNode *) (A)->data)
#define SUB(A,B) ( ((int) (A)) - ((int) (B)) )
#define POINTOFF(A,B) ( (struct Pointer *) (((int) A) + ((int) B)) )
#define NODEOFF(A,B) ( (struct EvalNode *) (((int) A) + ((int) B)) )
#define UNREL(A) {if((A)->tag==RELATIVE) {(A)->tag=ABSOLUTE; (A)->data=(unsigned)INDIRECTREL(A);};}
#define ISPOINTER(A) ((A)->tag == ABSOLUTE || (A)->tag == RELATIVE)
#define ADDREF(A) { if((A)->tag==ABSOLUTE) ++INDIRECTABS(A)->refcount; }
#define NEXTREL(A) (((A)->tag==NIL)?NULL:INDIRECTREL(A))

