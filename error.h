/* Asp applicative language
 * J.Cupitt, November 86
 * Exports for error handler */

extern void errorstart();		/* Start up error line */
extern void errorln();			/* Error message + line number */
extern void errorstr();			/* Add a string to error output */
extern void errornum();			/* Add a number */
extern void errorend();			/* End of error output */
extern void quit();			/* Exit */
extern void yyerror();			/* Yacc environment */

