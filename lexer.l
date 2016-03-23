%{
	/* Asp applicative language
	 * J.Cupitt, November 86
	 * Lexical analyser */
%}
%Start UNARY
%%
[\n\t\b ]				;
"%"					{ int ch;
					  while( (ch = input()) != EOF && 
							ch != '\n' )
						;
					  return( yylex() );
					}
error					{ BEGIN UNARY; return( TK_ERROR ); }
hd					{ BEGIN UNARY; return( TK_HD ); }
tl					{ BEGIN UNARY; return( TK_TL ); }
and					{ BEGIN UNARY; return( TK_AND ); }
or					{ BEGIN UNARY; return( TK_OR ); }
code					{ BEGIN UNARY; return( TK_CODE ); }
decode					{ BEGIN UNARY; return( TK_DECODE ); }
read					{ BEGIN UNARY; return( TK_READ ); }
write					{ BEGIN UNARY; return( TK_WRITE ); }
"#include"				{ BEGIN UNARY; return( TK_INCLUDE ); }
"[]"					{ yylval.cons = BuildNilConst();
					  BEGIN UNARY;
					  return( TK_CONST );
					}
\"\"					{ yylval.cons = BuildNilConst();	
					  BEGIN UNARY;
					  return( TK_CONST );
					}
<UNARY>"+"				return( TK_UPLUS );
<UNARY>"-"				return( TK_UMINUS );
"+"					|
"-"					|
"*"					|
"/"					|
":"					|
"="					|
"<"					|
">"					|
"["					|
"]"					|
"{"					|
"}"					|	
"("					|
","					|
"~"					{ BEGIN UNARY; return( *yytext ); }
">="					{ BEGIN UNARY; return( TK_MOREEQUAL ); }
"<="					{ BEGIN UNARY; return( TK_LESSEQUAL ); }
"<>"					{ BEGIN UNARY; return( TK_NOTEQUAL ); }
".."					{ BEGIN UNARY; return( TK_DOTS ); }
"."					{ BEGIN UNARY; return( *yytext ); }
")"					| 
";"					{ BEGIN 0;  return( *yytext ); }
\"[^"]*					{ if( yytext[ yyleng-1 ] == '\\')
						yymore();
					  else {
						input(); /* Skip closing " */
						yylval.cons = 
							BuildStringConst( yytext+1 );
						return( TK_CONST );
					   }
					   BEGIN UNARY;
					}
[0-9]+					{ yylval.cons = 
						BuildNumConst( atoi( yytext ) );
					  BEGIN 0;
					  return( TK_CONST );
					}
true					{ yylval.cons = BuildBoolConst( TRUE );
					  BEGIN UNARY;
					  return( TK_CONST );
					}
false					{ yylval.cons = 
						BuildBoolConst( FALSE );
					  BEGIN UNARY;
					  return( TK_CONST );
					}
"'"\\?."'"				{ yylval.cons = 
						BuildCharConst( yytext+1 );
					  BEGIN UNARY;
					  return( TK_CONST );
					}
[A-Za-z][A-Za-z0-9_\']*			{ yylval.cons = 
						BuildStringConst( yytext );
					  BEGIN 0;
					  return( TK_IDENTIFIER );
					}
.					{ errorln( "Illegal character '" );
					  errorstr( yytext );
					  errorstr( "'" );
					  errorend();
					  quit();
					}
%%


