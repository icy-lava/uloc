#define MAJOR 0
#define MINOR 3
#define PATCH 1

#define _STR(x) #x
#define STR(x) _STR(x)
#define VERSION (STR(MAJOR) "." STR(MINOR) "." STR(PATCH))

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include <stdio.h>

#ifdef _WIN32

#define MINIRENT_IMPLEMENTATION
#include "minirent.h"

// These are already defined in minirent.h:
// #define WIN32_LEAN_AND_MEAN
// #include <windows.h>

#ifdef _WIN64
#define ftello _ftelli64
#else
#define ftello ftell
#endif

#else
#include <dirent.h>
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
#define enumdef(name)   typedef enum   name name; enum   name

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

enumdef(OutputFormat) {
	OUTPUT_DEFAULT,
	OUTPUT_CSV,
	OUTPUT_TSV,
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
	
	if(pos == 0) return NULL;
	
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
	if(right.size < minSize) minSize = right.size;
	
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

void version(FILE *stream) {
	if(stream == NULL) stream = stderr;
	fprintf(stream, "uloc version %s\n", VERSION);
}

void usage(FILE *stream) {
	if(stream == NULL) stream = stderr;
	fputs(
		"Usage: uloc <file|directory|-option>...\n\n"
	, stream);
	version(stream);
	fputs(
		"\n"
		"Options:\n"
		"    -help     : show this help page\n"
		"    -version  : get uloc version information\n"
		"    -all      : don't ignore names that start with a dot\n"
		"    -fslash   : use forward-slashes as directory separators\n"
		"    -bslash   : use back-slashes as directory separators\n"
		"    -name     : use file name instead of full path for output\n"
		"    --        : interpret the rest of the args as files/directories\n"
		"\n"
		"Output options:\n"
		"    -noheader : don't output the header for csv/tsv formats\n"
		"    -csv      : output information as comma separated values\n"
		"    -tsv      : output information as tab separated values\n"
	, stream);
}

static void outputLineDefault(char *path, unat ulines, unat slines) {
	float percent = ulines * 100.0f / slines;
	printf("    %s: %zu/%zu : %.1f%%\n", path, ulines, slines, percent);
}

static void outputLineValues(OutputFormat outputFormat, FileInfo *finfo) {
	char *filepath = finfo->path.start;
	char *filename = finfo->name.start;
	char *fileext = finfo->ext.start;
	if(fileext == NULL) fileext = "none";
	
	unat ulines = finfo->lineCountUnique;
	unat slines = finfo->lineCount;
	float ratio = (float)ulines / slines;
	
	switch(outputFormat) {
		case OUTPUT_CSV: {
			printf("%s,%s,%s,%zu,%zu,%f\n", filepath, filename, fileext, ulines, slines, ratio);
		} break;
		case OUTPUT_TSV: {
			printf("%s\t%s\t%s\t%zu\t%zu\t%f\n", filepath, filename, fileext, ulines, slines, ratio);
		} break;
	}
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
	
	if(argc <= 1) {
		fputs("Error: got no arguments\n\n", stderr);
		usage(stderr);
		return 1;
	}
	
	FileInfo *files = NULL;
	
	#ifdef _WIN32
	char slash = '\\';
	#else
	char slash = '/';
	#endif
	
	bool dotfiles = false;
	bool outputHeader = true;
	bool nameOnly = false;
	
	OutputFormat outputFormat = OUTPUT_DEFAULT;
	
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
			
			if(matchInsensitive(arg, litToString("-version"))) {
				version(stdout);
				return 0;
			}
			
			if(matchInsensitive(arg, litToString("-all"))) {
				dotfiles = true;
				continue;
			}
			
			if(matchInsensitive(arg, litToString("-fslash"))) {
				slash = '/';
				continue;
			}
			
			if(matchInsensitive(arg, litToString("-bslash"))) {
				slash = '\\';
				continue;
			}
			
			if(matchInsensitive(arg, litToString("-name"))) {
				nameOnly = true;
				continue;
			}
			
			if(matchInsensitive(arg, litToString("-noheader"))) {
				outputHeader = false;
				continue;
			}
			
			if(matchInsensitive(arg, litToString("-csv"))) {
				outputFormat = OUTPUT_CSV;
				continue;
			}
			
			if(matchInsensitive(arg, litToString("-tsv"))) {
				outputFormat = OUTPUT_TSV;
				continue;
			}
			
			fprintf(stderr, "Error: unknown option %s\n\n", argv[i]);
			usage(stderr);
			return 1;
		}
		
		if(arg.size == 0) {
			fprintf(stderr, "Error: file path must not be empty\n\n");
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
	
	/////////////////////////////////
	/// Find files in directories ///
	
	for(int i = 0; i < arrlen(files); i++) {
		FileInfo *finfo = files + i;
		String path = finfo->path;
		
		DIR *dir = opendir(path.start);
		
		// NOTE: If dir is NULL, it's probably not a directory.
		// If it doesn't exist at all, we'll print a warning later.
		if(dir != NULL) {
			struct dirent *ent;
			while((ent = readdir(dir)) != NULL) {
				if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
				
				// I assume we can't have 0 length filenames
				if(!dotfiles && ent->d_name[0] == '.') continue;
				
				unat d_namlen = strlen(ent->d_name);
				
				// Create new path with file name appended
				char *newPath = malloc(path.size + d_namlen + 1 + 1);
				char *pointer = newPath;
				
				memcpy(pointer, path.start, path.size);
				pointer += path.size;
				
				// Avoid separating with a slash if there's already a slash there
				if(pointer[-1] != '/' && pointer[-1] != '\\') {
					newPath[path.size] = slash;
					pointer++;
				}
				
				memcpy(pointer, ent->d_name, d_namlen);
				pointer += d_namlen;
				
				unat newPathSize = pointer - newPath;
				newPath[newPathSize] = 0;
				
				String newPathString = {
					.size = newPathSize,
					.start = newPath
				};
				
				// Add to file list. We don't know if it's a directory yet, but it'll get taken
				// care of in the outer loop, since we're pushing this onto the end of the list.
				arrput(files, (FileInfo) {
					.path = newPathString
				});
			}
			
			// It's clearly not a file, so remove it
			arrdel(files, i);
			i--;
			
			closedir(dir);
		}
	}
	
	/// Find files in directories ///
	/////////////////////////////////
	
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
			fprintf(stderr, "    %s: %s\n", nameOnly ? finfo->name.start : filepath, errorMessage);
		}
		
		// Remove file from our list if we couldn't read it or it was empty
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
	
	if(arrlen(files) == 0) {
		fputs("Error: no files to scan\n\n", stderr);
		usage(stderr);
		return status;
	}
	
	/// File reading ///
	////////////////////
	
	String *lines = NULL;
	unat totalLineCount = 0;
	
	////////////////////////////////
	/// Line counting and output ///
	
	switch(outputFormat) {
		case OUTPUT_DEFAULT: {
			printf("Unique lines:\n");
		} break;
		case OUTPUT_CSV: {
			if(outputHeader) {
				puts("filepath,filename,fileext,unique lines,source lines,ratio");
			}
		} break;
		case OUTPUT_TSV: {
			if(outputHeader) {
				puts("filepath\tfilename\tfileext\tunique lines\tsource lines\tratio");
			}
		} break;
	}
	
	for(int i = 0; i < arrlen(files); i++) {
		FileInfo *finfo = files + i;
		FileData *fdata = finfo->data;
		
		// Count number of lines which are not whitespace only, and put them into the lines array
		char *start = fdata->data;
		char *stop = start;
		char *end = fdata->data + fdata->size;
		while(stop < end) {
			String line = {
				.start = start
			};
			
			// Keep moving up `stop` until LF or EOF
			for(; stop < end && stop[0] != '\n'; stop++) {}
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
		
		switch(outputFormat) {
		case OUTPUT_DEFAULT: {
			outputLineDefault(nameOnly ? finfo->name.start : finfo->path.start, finfo->lineCountUnique, finfo->lineCount);
		}
		case OUTPUT_CSV:
		case OUTPUT_TSV:
			outputLineValues(outputFormat, finfo);
		}
		
		totalLineCount += finfo->lineCount;
	}
	
	qsort(lines, totalLineCount, sizeof(*lines), compareLines);
	
	unat totalLineCountUnique = totalLineCount != 0;
	for(unat j = 1; j < totalLineCount; j++) {
		String *line = lines + j;
		String *prev = line - 1;
		if(compareStrings(*prev, *line) != 0) totalLineCountUnique++;
	}
	
	if(outputFormat == OUTPUT_DEFAULT) {
		putc('\n', stdout);
		outputLineDefault("total", totalLineCountUnique, totalLineCount);
	}
	
	/// Line counting and output ///
	////////////////////////////////
	
	return status;
}