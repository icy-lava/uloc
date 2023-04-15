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

structdef(String) {
	unat size;
	char *start;
};

#define litToString(lit) ((String){.size = sizeof(lit) - 1, .start = lit})
static inline String cstrToString(const char *cstr) {
	return (String){
		.size = strlen(cstr),
		.start = (char*)cstr
	};
}

structdef(FileInfo) {
	String path;
	String name;
	String ext;
	
	unat lineCount;
	unat lineCountUnique;
	
	FileData *data;
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
	uloc_assert(fread(fdata->data, pos, 1, file) == 1, errorMessage, "failed to read file");
	
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

bool matchInsensitive(String a, String b) {
	if(a.size != b.size) return false;
	
	for(unat i = 0; i < a.size; i++) {
		if(toLower(a.start[i]) != toLower(b.start[i])) return false;
	}
	
	return true;
}

static inline int compareStrings(const String left, const String right) {
	unat minSize = left.size;
	if(right.size > minSize) minSize = right.size;
	
	int comparison = memcmp(left.start, right.start, minSize);
	if(comparison == 0) {
		if(left.size < right.size) return -1;
		return left.size > right.size;
	}
	
	return comparison;
}

static int compareLines(const void *a, const void *b) {
	return compareStrings(*(const String*)a, *(const String*)b);
}

void usage(FILE *stream) {
	if(stream == NULL) stream = stderr;
	fprintf(stream, "Usage: uloc <file|directory>...\n");
	fputc('\n', stream);
	fprintf(stream, "Options:\n");
	fprintf(stream, "    -help : show this help page\n");
	fprintf(stream, "    --    : interpret the rest of the args as files/directories\n");
}

int main(int argc, char *argv[]) {
	
	////////////////////////////////////////
	/// Enable UTF8 binary IO on Windows ///
	
	// NOTE: manifest.xml should be applied to the executable, so that argv is UTF8 rather than ASCII
	
	#ifdef _WIN32
		_setmode(0, 0x8000);
		_setmode(1, 0x8000);
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);
	#endif
	
	/// Enable UTF8 binary IO on Windows ///
	////////////////////////////////////////
	
	FileInfo *files = NULL;
	
	////////////////////////////
	/// CLI argument parsing ///
	
	bool wantOptions = true;
	
	for(int i = 1; i < argc; i++) {
		String arg = cstrToString(argv[i]);
		if(wantOptions && arg.size > 0 && arg.start[0] == '-') {
			if(compareStrings(arg, litToString("-")) == 0) {
				// TODO: handle stdin input
				continue;
			}
			
			if(compareStrings(arg, litToString("--")) == 0) {
				wantOptions = false;
				continue;
			}
			
			// Tolerate double dashes
			if(arg.size >= 2 && arg.start[1] == '-') {
				arg.size--;
				arg.start++;
			}
			
			if(matchInsensitive(arg, litToString("-help"))) {
				usage(stdout);
				return 0;
			}
			
			fprintf(stderr, "Error: unknown option %s\n", argv[i]);
			usage(stderr);
			return 1;
		}
		
		if(arg.size == 0) {
			fprintf(stderr, "Error: file path must not be empty\n");
			usage(stderr);
			return 1;
		}
		
		FileInfo finfo = {
			.path = arg
		};
		arrput(files, finfo);
	}
	
	/// CLI argument parsing ///
	////////////////////////////
	
	////////////////////////////////////
	/// Find file name and extension ///
	
	for(int i = 0; i < arrlen(files); i++) {
		FileInfo *finfo = files + i;
		char *filepath = finfo->path.start;
		unat pathLength = finfo->path.size;
		
		// Find filename by iterating backwards and checking for a slash/bslash
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
		
		finfo->name = (String) {
			.size = strlen(filename),
			.start = filename
		};
		
		finfo->ext = (String) {
			.size = extension ? strlen(extension) : 0,
			.start = extension
		};
	}
	
	/// Find file name and extension ///
	////////////////////////////////////
	
	int status = 0;
	
	////////////////////
	/// File reading ///
	
	for(int i = 0; i < arrlen(files); i++) {
		FileInfo *finfo = files + i;
		char *filepath = finfo->path.start;
		
		char *errorMessage = NULL;
		FileData *fdata = readFile(filepath, &errorMessage);
		
		if(errorMessage != NULL) {
			if(status == 0) {
				fprintf(stderr, "File errors:\n");
				status = 1;
			}
			fprintf(stderr, "    %s: %s\n", filepath, errorMessage);
		}
		
		// Remove file from our list if we couldn't read it
		if(fdata == NULL) {
			arrdel(files, i);
			i--;
			continue;
		}
		
		finfo->data = fdata;
	}
	
	if(status != 0) {
		fputc('\n', stderr);
		fflush(stderr);
	}
	
	/// File reading ///
	////////////////////
	
	String *lines = NULL;
	unat totalLineCount = 0;
	
	/////////////////////
	/// Line counting ///
	
	printf("Unique lines:\n");
	
	for(int i = 0; i < arrlen(files); i++) {
		FileInfo *finfo = files + i;
		FileData *fdata = finfo->data;
		
		// Count number of lines which are not whitespace only, and put them into the lines array
		char *start = fdata->data;
		char *stop = start;
		while((stop - fdata->data) < fdata->size) {
			String line = {
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
				finfo->lineCount++;
			}
			
			stop = start;
		}
		
		qsort(lines + totalLineCount, finfo->lineCount, sizeof(*lines), compareLines);
		
		finfo->lineCountUnique = finfo->lineCount != 0;
		for(unat j = 1; j < finfo->lineCount; j++) {
			String *line = lines + totalLineCount + j;
			String *prev = line - 1;
			if(compareStrings(*prev, *line) != 0) finfo->lineCountUnique++;
		}
		
		float percent = finfo->lineCountUnique * 100.0f / finfo->lineCount;
		printf("    %s: %zu/%zu (%4.1f%%)\n", finfo->path.start, finfo->lineCountUnique, finfo->lineCount, percent);
		
		totalLineCount += finfo->lineCount;
	}
	
	putc('\n', stdout);
	
	qsort(lines, totalLineCount, sizeof(*lines), compareLines);
	
	unat totalLineCountUnique = totalLineCount != 0;
	for(unat j = 1; j < totalLineCount; j++) {
		String *line = lines + j;
		String *prev = line - 1;
		if(compareStrings(*prev, *line) != 0) totalLineCountUnique++;
	}
	
	float percent = totalLineCountUnique * 100.0f / totalLineCount;
	printf("    total: %zu/%zu (%4.1f%%)\n", totalLineCountUnique, totalLineCount, percent);
	
	/// Line counting ///
	/////////////////////
	
	return status;
}