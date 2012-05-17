# Makefile for Tools

CC = $(CROSS_COMPILE)gcc

CFLAGS += -Wall -Wextra -O2

all: isp prog

isp: isp_main.c isp_utils.c isp_commands.c
	$(CC) $(CFLAGS) $^ -o $@

prog: lpc_prog.c isp_utils.c isp_commands.c prog_commands.c parts.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f isp
	rm -f prog
