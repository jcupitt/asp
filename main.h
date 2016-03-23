/* Asp applicative language
 * J.Cupitt, November 86
 * Main section exports */

extern enum Boolean Warnings;		/* Warnings enabled */
extern enum Boolean Extras;		/* Extra messages enabled */
extern enum Boolean Counting;		/* Counts turned on */
extern char *OurName;			/* Name we were invoked by */
extern int EvalHeapSize;		/* Size of eval heap */
extern int ParseHeapSize;		/* And size of parse heap */
extern int AStackSize;			/* Size of application stack */
extern int FStackSize;			/* Size of fence stack */
extern int PStackSize;			/* Size of pointer stack */
