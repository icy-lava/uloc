.PHONY: run build clean

run: uloc.exe
	uloc.exe uloc.c stb_ds.h Makefile manifest.xml

build: uloc.exe

clean:
	busybox rm -f uloc.exe uloc.obj uloc.o

uloc.exe: uloc.c
	cl.exe /O2 /nologo /Fe:uloc.exe uloc.c && mt.exe /nologo /manifest manifest.xml /outputresource:uloc.exe;#1