#!/bin/sh
.SILENT:

default:
	gcc -g prog.c -o lab3a

dist:
	tar -cvf lab3a-704666892.tar.gz prog.c README Makefile

clean: default
	-rm -f lab3a
