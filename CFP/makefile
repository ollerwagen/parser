SHELL = /bin/bash

HEADERS = hdr
SOURCES = src

COMPILER = g++

CFLAGS = -std=c++2a
WFLAGS = -Wall
OFLAGS = -O

FLAGS = $(CFLAGS) $(WFLAGS) $(OFLAGS)

make:
	$(COMPILER) -o parser -I $(HEADERS) $(SOURCES)/*.cpp $(FLAGS)