/* Asp applicative langauge
 * J.Cupitt, November 86
 * Exports for memory allocator */

extern void InitMemory();		/* Set up and clear heaps */
extern char *allocate();		/* Allocate from parseheap */
#define GetVec(A) ((A *)allocate(sizeof(A)))
extern char *strsave();			/* Save a str */
extern int UsedSpace();			/* Find % of space used */
extern int NumberCompacts;		/* Note number of compacts */
extern struct EvalNode *GetTop();	/* Find end of heap */
extern struct EvalNode *GetCell();	/* Allocate a cell */
extern struct EvalNode *GetBlock();	/* Allocate a group of cells */
extern void ForAllCell();		/* Scan heap in range */
extern void ForHeap();			/* Scan heap */
