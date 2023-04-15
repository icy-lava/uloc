.PHONY: run build clean

ifeq ($(OS),Windows_NT)
BINARY:=uloc.exe

$(BINARY): uloc.c
	build.bat

run: $(BINARY)
	$< .
else
BINARY:=uloc

$(BINARY): uloc.c
	gcc -O3 uloc.c -o $@

run: $(BINARY)
	./$< .
endif

build: $(BINARY)

clean:
	busybox rm -f $(BINARY) uloc.obj uloc.o uloc