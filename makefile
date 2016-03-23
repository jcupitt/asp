# Asp appliactive language
# J.Cupitt, November 86

OBJECTS = error.o io.o main.o memory.o symbol.o tree.o y.tab.o dump.o \
	  compile.o evaluate.o subexpr.o declare.o list.o

HEADERS = asphdr.h lextypes.h parsetypes.h declare.h parser.h list.h \
	  symboltypes.h error.h io.h memory.h symbol.h tree.h dump.h \
	  evaltypes.h compile.h pointermacros.h evaluate.h makefile \
	  main.h subexpr.h

.c.o: ; cc -O -c $*.c 

asp : $(OBJECTS)
	cc -O -o asp $(OBJECTS)
	echo 'Done.'

y.tab.c : parser.y lexer.l $(HEADERS)
	lex lexer.l
	yacc parser.y

$(OBJECTS) : $(HEADERS)
