/* Asp applicative language
 * J.Cupitt, November 86
 * Exports for symbol table functions */

extern struct Symbol *MakeRoot();		/* Make root symbol */
extern struct SymbolTable *CreateTable();	/* Make a new table */
extern void ForAllSym();			/* Apply fn to a table */
extern struct Symbol *AddName();		/* Add name to table */
extern struct Symbol *FindName();		/* Find a name in a table */
extern void AddBody();				/* Add a body */
extern void AddTable();				/* Add a table */
extern void AddReference();			/* Add a reference */
extern void AddParam();				/* Add a param */
extern struct SymbolTable *GlobalTable;		/* Root table */
extern void PushSymbol();			/* Push sym */
extern struct Symbol *PopSymbol();		/* Pop sym */
extern void ResolveNames();			/* Pass out all unresolveds */
