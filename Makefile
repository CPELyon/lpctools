# Makefile for Tools

CC = $(CROSS_COMPILE)gcc

CFLAGS += -Wall -Wextra -O2

all: isp prog

isp: isp_main.o isp_utils.o isp_commands.o isp_wrapper.o

prog: lpc_prog.o isp_utils.o isp_commands.o prog_commands.o parts.o



isp_main.o: isp_utils.h isp_commands.h

isp_utils.o:

isp_commands.o: isp_utils.h

isp_wrapper.o: isp_utils.h isp_commands.h

lpc_prog.o: isp_utils.h isp_commands.h prog_commands.h parts.h

prog_commands.o: isp_utils.h isp_commands.h parts.h

parts.o: parts.h



clean:
	rm -f *.o
mrproper: clean
	rm -f isp
	rm -f prog
