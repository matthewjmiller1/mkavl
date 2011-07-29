#
# Makefile: Build and clean the program
# Copyright (C) 2011  Matt Miller
#
# Based on example from:
# http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

#IDIR =../include
CC=gcc
#CFLAGS=-I$(IDIR)
CFLAGS=-Wall -g

ODIR=obj
#LDIR =../lib

#LIBS=-lm
LIBS=

DIR=libavl
NAME=$(DIR)

#_DEPS = hellomake.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))
DEPS = avl.h test.h

_OBJ = avl.o avl_test.o test.o 
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

test_$(NAME): $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

all: test_$(NAME)

.PHONY: clean tar doc

clean:
	rm -f test_$(NAME) $(ODIR)/*.o *~ core 

doc:
	doxygen
	cd doc/latex; pdflatex refman.tex

tar:
	tar -czvf $(NAME).tar.gz ../$(NAME) --exclude *.swp --exclude *.o \
        --exclude test_$(NAME) --exclude $(NAME).tar.gz
