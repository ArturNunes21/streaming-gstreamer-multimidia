# Terminal MSYS2 UCRT64 ou Linux: make
CC      = gcc
CFLAGS  = -Wall -Wextra
GST     = $(shell pkg-config --cflags --libs gstreamer-1.0)
GUI     = $(shell pkg-config --cflags --libs gtk+-3.0 gstreamer-1.0 gio-2.0)
PROGRAMAS = transmissor receptor servidor cliente

all: $(PROGRAMAS)

transmissor: transmissor.c
	$(CC) $(CFLAGS) $< -o $@ $(GST)

receptor: receptor.c
	$(CC) $(CFLAGS) $< -o $@ $(GST)

servidor: servidor.c midia.c midia.h
	$(CC) $(CFLAGS) servidor.c midia.c -o $@ $(GUI)

cliente: cliente.c midia.c midia.h
	$(CC) $(CFLAGS) cliente.c midia.c -o $@ $(GUI)

clean:
	rm -f $(PROGRAMAS) $(PROGRAMAS:=.exe)

.PHONY: all clean
