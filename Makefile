.PHONY: run build clean

ifeq ($(OS),Windows_NT)
BINARY:=uloc.exe

$(BINARY): uloc.c
	build.bat
else
BINARY:=uloc

$(BINARY): uloc.c
	gcc -Werror -O3 uloc.c -o $@
endif

build: $(BINARY)

run: $(BINARY)
	./$< .

clean:
	busybox rm -f "$(BINARY)" uloc.obj uloc.o uloc