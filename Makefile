GCC = gcc
OUT = dwmstatus
IN = $(OUT).c
all: $(IN)
	$(GCC) -Wall -pedantic -std=c99 $(IN) -o $(OUT) -std=c99 -lX11 -lasound
clean:
	rm -f dwmstatus
