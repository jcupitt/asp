/* Asp applicative language
 * J.Cupitt, November 86
 * List macros & exports for list.c */

#define hd(A) (((A)==NULL)?NULL:((A)->this))
#define tl(A) (((A)==NULL)?NULL:((A)->next))
extern struct ExpressionList *econs();
extern struct PatchList *pcons();
extern struct SymbolList *scons();
extern struct PatchList *pcat();
extern void PatchRefs();
