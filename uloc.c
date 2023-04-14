#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include <stdio.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef _WIN64
#define ftello _ftelli64
#else
#define ftello ftell
#endif

#endif

#include <stdint.h>

typedef  int64_t  int8;
typedef  int32_t  int4;
typedef  int16_t  int2;
typedef   int8_t  int1;
typedef uint64_t uint8;
typedef uint32_t uint4;
typedef uint16_t uint2;
typedef  uint8_t uint1;

typedef intptr_t  nat;
typedef   size_t unat;

typedef int bool;
#define  true 1
#define false 0

#define structdef(name) typedef struct name name; struct name

structdef(FileData) {
	unat size;
	char data[0];
};

structdef(Line) {
	unat size;
	char *start;
};

structdef(FileInfo) {
	char *path;
	char *name;
	char *ext;
	
	unat lineCount;
	unat lineCountUnique;
	
	FileData *data;
	Line     *lines;
};

#define uloc_error(var, message) do {assert((var) != NULL); *(var) = (message); return NULL;} while(0)
#define uloc_assert(condition, var, message) do {assert((var) != NULL); if(!(condition)) uloc_error((var), (message));} while(0)

FileData *readFile(char *path, char **errorMessage) {
	FILE *file = fopen(path, "rb");
	uloc_assert(file != NULL, errorMessage, "could not open file");
	
	// Get file size first
	fseek(file, 0, SEEK_END);
	unat pos = ftello(file);
	fseek(file, 0, SEEK_SET);
	
	// Allocate enough memory for size + data
	FileData *fdata = malloc(sizeof(FileData) + pos);
	uloc_assert(fdata != NULL, errorMessage, "could not allocate memory");
	
	// Assign size
	fdata->size = pos;
	
	// Read contents of file to memory
	uloc_assert(fread(fdata->data, pos, 1, file) == 1, errorMessage, "failed to read from file");
	
	fclose(file);
	
	return fdata;
}

static inline char toLower(char c) {
	if(c >= 'A' && c <= 'Z') c += 32;
	return c;
}

static inline bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r';
}

bool matchInsensitive(char *a, char *b) {
	unat alen = strlen(a);
	unat blen = strlen(b);
	
	if(alen != blen) return false;
	
	for(unat i = 0; i < alen; i++) {
		if(toLower(a[i]) != toLower(b[i])) return false;
	}
	
	return true;
}

static inline int compareLines(const void *a, const void *b) {
	const Line *lineA = a;
	const Line *lineB = b;
	
	unat minSize = lineA->size;
	if(lineB->size > minSize) minSize = lineB->size;
	
	int comparison = memcmp(lineA->start, lineB->start, minSize);
	if(comparison == 0) {
		if(lineA->size < lineB->size) return -1;
		return lineA->size > lineB->size;
	}
	
	return comparison;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
	_setmode(0, 0x8000);
	_setmode(1, 0x8000);
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
#endif
	
	int status = 0;
	
	FileInfo *files = NULL;
	Line *lines = NULL;
	unat totalLineCount = 0;
	
	for(int i = 1; i < argc; i++) {
		char *filepath = argv[i];
		unat pathLength = strlen(filepath);
		
		char *errorMessage = NULL;
		FileData *fdata = readFile(filepath, &errorMessage);
		if(errorMessage != NULL) {
			if(status == 0) {
				fprintf(stderr, "File errors:\n");
				status = 1;
			}
			fprintf(stderr, "    %s: %s\n", filepath, errorMessage);
			continue;
		}
		if(fdata == NULL) continue;
		
		// Find filename by iterating backwards and checking for slash/bslash
		char *filename = filepath + pathLength - 1;
		for(; filename >= filepath; filename--) {
			if(filename[0] == '/' || filename[0] == '\\') {
				break;
			}
		}
		filename++;
		
		// Find file extension by iterating backwards and checking for a '.'
		char *extension = filepath + pathLength - 1;
		bool foundExtension = false;
		for(; extension >= filename; extension--) {
			if(extension[0] == '.') {
				foundExtension = true;
				break;
			}
		}
		
		if(!foundExtension) {
			extension = NULL;
		}
		
		FileInfo finfo = {
			.path = filepath,
			.name = filename,
			.ext  = extension,
			
			.lineCount = 0,
			.lineCountUnique = 0
		};
		
		// Line counting
		{
			char *start = fdata->data;
			char *stop = start;
			while((stop - fdata->data) < fdata->size) {
				Line line = {
					.start = start
				};
				
				// Keep moving up `stop` until LF or EOF
				for(; stop[0] != '\n' && (stop - fdata->data) < fdata->size; stop++) {}
				start = stop + 1;
				
				for(; line.start < stop; line.start++) {
					if(!isWhitespace(line.start[0])) {
						break;
					}
				}
				
				for(; stop > line.start; stop--) {
					if(!isWhitespace(stop[0])) {
						break;
					}
				}
				
				if(line.start < stop) {
					line.size = stop - line.start;
					arrput(lines, line);
					finfo.lineCount++;
				}
				
				stop = start;
			}
		}
		
		qsort(lines + totalLineCount, finfo.lineCount, sizeof(*lines), compareLines);
		
		finfo.lineCountUnique = finfo.lineCount != 0;
		for(unat j = 1; j < finfo.lineCount; j++) {
			Line *line = lines + totalLineCount + j;
			Line *prev = line - 1;
			if(compareLines(prev, line) != 0) finfo.lineCountUnique++;
		}
		
		float percent = finfo.lineCountUnique * 100.0f / finfo.lineCount;
		printf("file '%s': %zu/%zu unique lines (%4.1f%%)\n", filepath, finfo.lineCountUnique, finfo.lineCount, percent);
		
		totalLineCount += finfo.lineCount;
		
		arrput(files, finfo);
	}
	
	qsort(lines, totalLineCount, sizeof(*lines), compareLines);
	
	unat totalLineCountUnique = totalLineCount != 0;
	for(unat j = 1; j < totalLineCount; j++) {
		Line *line = lines + j;
		Line *prev = line - 1;
		if(compareLines(prev, line) != 0) totalLineCountUnique++;
	}
	
	float percent = totalLineCountUnique * 100.0f / totalLineCount;
	printf("total: %zu/%zu unique lines (%4.1f%%)\n", totalLineCountUnique, totalLineCount, percent);
	
	return status;
}