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

LIBS=-lmkavl

NAME=mkavl
LIB_NAME=lib$(NAME).so

#DEPS =

_OBJ = test_$(NAME).o 
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

test: test_$(NAME)

test_$(NAME): $(OBJ) $(LDIR)/$(LIB_NAME) 
	$(CC) -o $@ $< $(CFLAGS) -Wl,-rpath=$(LDIR) -L $(LDIR) $(LIBS)

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core test_$(NAME)
