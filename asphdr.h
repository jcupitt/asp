/* Asp applicative language.
 * J.Cupitt,  November '86
 * Main header file - #included by all files. */

#include <stdio.h>			/* Useful ! */

/* An enum for flags */
enum Boolean {
	FALSE, TRUE
};

/* #define DEBUG for general debugging output */
/* #define TREES for dumping of trees at regular intervals.
 * Acyclic graphs only for this one! */
