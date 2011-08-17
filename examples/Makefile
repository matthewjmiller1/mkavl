#
# Makefile: Build and clean the program
# Copyright (C) 2011  Matt Miller
#
# Based on example from:
# http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

#IDIR =../include
CC=gcc
#CFLAGS=-I$(IDIR)
CFLAGS=-Wall -Werror -g

ODIR=obj
LDIR=../lib

LIBS=-lmkavl -lm

LIB_NAME=libmkavl.so

DEPS = examples_common.h

_EMPLOYEE_DB_OBJ = employee_example.o
EMPLOYEE_DB_OBJ = $(patsubst %,$(ODIR)/%,$(_EMPLOYEE_DB_OBJ))

_MALLOC_OBJ = malloc_example.o
MALLOC_OBJ = $(patsubst %,$(ODIR)/%,$(_MALLOC_OBJ))

all: employee_example malloc_example

malloc_example: $(MALLOC_OBJ) $(DEPS) $(LDIR)/$(LIB_NAME) 
	$(CC) -o $@ $< $(CFLAGS) -Wl,-rpath=$(LDIR) -L $(LDIR) $(LIBS)

employee_example: $(EMPLOYEE_DB_OBJ) $(DEPS) $(LDIR)/$(LIB_NAME) 
	$(CC) -o $@ $< $(CFLAGS) -Wl,-rpath=$(LDIR) -L $(LDIR) $(LIBS)

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core employee_example malloc_example
