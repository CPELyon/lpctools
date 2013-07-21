# Makefile for Tools

CC = $(CROSS_COMPILE)gcc

CFLAGS += -Wall -Wextra -O2

all: lpcisp lpcprog

lpcisp: lpcisp.o isp_utils.o isp_commands.o isp_wrapper.o

lpcprog: lpcprog.o isp_utils.o isp_commands.o prog_commands.o parts.o



lpcisp.o: isp_utils.h isp_commands.h

isp_utils.o:

isp_commands.o: isp_utils.h

isp_wrapper.o: isp_utils.h isp_commands.h

lpcprog.o: isp_utils.h isp_commands.h prog_commands.h parts.h

prog_commands.o: isp_utils.h isp_commands.h parts.h

parts.o: parts.h



clean:
	rm -f *.o
mrproper: clean
	rm -f lpcisp
	rm -f lpcprog
