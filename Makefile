# Makefile for Tools

CC = $(CROSS_COMPILE)gcc

CFLAGS += -Wall -Wextra -O2

all: isp

isp: isp_main.c isp_utils.c isp_commands.c
	$(CC) $(CFLAGS) $^ -o $@

