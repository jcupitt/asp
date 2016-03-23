/* Asp applicative language
   J.Cupitt, November 86
   Exports for io file. */

extern int input();			/* Lex io */
extern void output();
extern void unput();
extern int yywrap();
extern void ParseFiles();		/* Parse list of files */
extern int lineno;			/* Current line number */
extern char *filename;			/* Current file name */
extern void AddFile();			/* Add a new file to list awaiting */
