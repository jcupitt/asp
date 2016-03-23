/* Asp applicative language
 * J.Cupitt, November 86
 * Exports for the evaluator */

extern void EvaluateScript();		/* Reduce graph! */
extern void InitEvalStacks();		/* Allocate space for stacks */
extern struct EvalNode **Stack;		/* Runtime argument stack */
extern int sp;				/* Stack pointer */
extern struct EvalNode ***PointerStack;	/* Pointers into the heap */
extern int psp;				/* SP for this */
extern int NumberReductions;		/* Number of transformations */
