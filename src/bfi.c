/*
 *	Copyright (c) 2006, 2016, David DeHaven
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions are met:
 *		*	Redistributions of source code must retain the above copyright 
 *			notice, this list of conditions and the following disclaimer.
 *		*	Redistributions in binary form must reproduce the above copyright
 *			notice, this list of conditions and the following disclaimer in the
 *			documentation and/or other materials provided with the distribution.
 *		*	Neither the name of the author nor the names of its
 *			contributors may be used to endorse or promote products derived from
 *			this software without specific prior written permission.
 *
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *	POSSIBILITY OF SUCH DAMAGE.
 */

// the following line allows compilation using gcc under Linux
#define _POSIX_C_SOURCE 200810
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <termios.h>


#define DEFAULT_MEMSIZE 30000

// FIXME: this should be moved to a separate module, or I could switch to C++

/* Brainfuck VM */
typedef struct {
	size_t Sp;	// source pointer
	size_t sourceSize;
	unsigned char *sourcePool;

	// FIXME: probably more efficient to just use a pointer for this and Sp
	int Hp;		// heap pointer
	int heapSize;
	unsigned char *heap;
} bfvm_s; 

/*
 * Allocate a new bfi VM with the given array size.
 */
bfvm_s* bfvm_alloc(int size)
{
	bfvm_s *vm = calloc(1, sizeof(bfvm_s));
	if (vm) {
		vm->heap = calloc(1, size);
		if (!vm->heap) {
			fprintf(stderr, "Unable to allocate bf vm of size %d\n", size);
			free(vm);
			return NULL;
		}
		vm->heapSize = size;
	}
	return vm;
}

void bfvm_free(bfvm_s *vm)
{
	if (vm) {
		if (vm->sourcePool) {
			free(vm->sourcePool);
			vm->sourcePool = NULL; // stale ptr protection
		}
		if (vm->heap) {
			free(vm->heap);
			vm->heap = NULL;
		}
		free(vm);
	}
}

void bfvm_reset(bfvm_s *vm)
{
	if (vm) {
		vm->Sp = 0;
		vm->Hp = 0;
		if (vm->heap) {
			memset(vm->heap, 0, vm->heapSize);
		}
	}
}

void bfvm_run(bfvm_s *vm)
{
	bfvm_reset(vm);
	while(vm->Sp < vm->sourceSize) {
		int temp;
		
		switch(vm->sourcePool[vm->Sp]) {
			case '>':
				vm->Hp++;
				if(vm->Hp >= vm->heapSize) vm->Hp = 0;
				break;
			
			case '<':
				if(vm->Hp == 0) vm->Hp = vm->heapSize;
				vm->Hp--;
				break;
			
			case '+':
				vm->heap[vm->Hp]++;
				break;
			
			case '-':
				vm->heap[vm->Hp]--;
				break;
			
			case '.':
				putchar(vm->heap[vm->Hp]);
				break;
			
			case ',':
				temp = getchar();
				if(temp != 0x04) {	/* ^D sends 0x04 (EOT) in the mode we're running in */
					vm->heap[vm->Hp] = (unsigned char)temp;
				} else {
					vm->heap[vm->Hp] = 0;
				}
				putchar(vm->heap[vm->Hp]);
				fflush(stdout);
				break;
			
			case '[':
				temp = 1; /* nesting level */
				if(vm->heap[vm->Hp] == 0) {
					vm->Sp++;
					while(vm->Sp < vm->sourceSize) {
						if(vm->sourcePool[vm->Sp] == ']') {
							temp--;
							if(temp == 0) break; /* found matching brace */
						} else if(vm->sourcePool[vm->Sp] == '[') {
							temp++;
						}
						vm->Sp++;
					}
				}
				break;
			
			case ']':
				temp = 1;
				if(vm->heap[vm->Hp] != 0) {
					vm->Sp--;
					while(vm->Sp > 0) {
						if(vm->sourcePool[vm->Sp] == '[') {
							temp--;
							if(temp == 0) break; /* found matching brace */
						} else if(vm->sourcePool[vm->Sp] == ']')
							temp++;
						vm->Sp--;
					}
				}
				break;

			// TODO: strict mode switch
			default:
				break;
		}
		
		vm->Sp++;
	}
}

// FIXME: termios is antiquated and platform dependent...
static struct termios savedTerm;

void exit_handler()
{
	/* reset terminal settings */
	tcsetattr(0, TCSANOW, &savedTerm);
}

int load_program(bfvm_s *vm, FILE *fp)
{
	void *progBuf = NULL;
	size_t progSize = 0;
	struct stat fps;

	if (!fp || !vm) return 1;

	// st_size may be indeterminate (stdin), so don't count on fstat actually returning anything useful
	int st = fstat(fileno(fp), &fps);
	if (st == 0 && (fps.st_size > 0)) {
		printf("stat size = %d\n", fps.st_size);
		
		// TODO: mmap instead of reading explicitly
		progSize = (size_t)fps.st_size;
		progBuf = calloc(1, progSize);
		
		if (fread(progBuf, progSize, 1, fp) != 1) {
			fprintf(stderr, "Error reading from file\n");
			free(progBuf);
			return 1;
		}
	} else {
		// pipe or stdin, read blocks of chars until we hit EOF
		char buf[1024];
		// use calloc to avoid running garbage, in case the interpreter overflows
		progSize = 1024;
		progBuf = calloc(1, progSize);
		size_t offset = 0;
		size_t count;
		do {
			count = fread(buf, 1, 1024, fp);
			if (count > 0) {
				size_t growSize = progSize;
				while (offset + count >= growSize) {
					growSize <<= 1; // grow exponentially to reduce allocation count
				}
				if (progSize != growSize) {
					void *newBuf = calloc(1, growSize);
					if (!newBuf) {
						free(progBuf);
						return 1;
					}
					memcpy(newBuf, progBuf, offset);
					free(progBuf);
					progBuf = newBuf;
					progSize = growSize;
				}
				memcpy(progBuf + offset, buf, count);
				offset += count;
			}
		} while(!feof(fp));

		if (offset < progSize) {
			progSize = offset; // trim any excess
		}
	}

	vm->sourcePool = progBuf;
	vm->sourceSize = progSize;
	printf("\nRead %d chars into program memory\n", progSize);

	return 0;
}

/*
 * We take one optional argument, the program file to load and execute.
 * If no argument is given or "-" is passed, then we try loading the program from stdin.
 */
int main(int argc, char **argv)
{
	struct termios newTerm;
	int rval = 0;
	
	// FIXME: Add options parsing using something getopt-ish
	// FIXME: Allow specifying vm size and behavior
	bfvm_s *vm = bfvm_alloc(DEFAULT_MEMSIZE);
	if(argc >= 2 && !(argv[1][0] == '-' && argv[1][1] == '\0')) {
		FILE *fp = fopen(argv[1], "r");
		if (fp) {
			printf("Reading program from file %s\n", argv[1]);
			rval = load_program(vm, fp);
			fclose(fp);
		} else {
			fprintf(stderr, "Unable to open file \"%s\"\n", argv[1]);
			return 1;
		}
	} else {
		rval = load_program(vm, stdin);
	}

	if (rval == 0) {
		/* set term settings to unbuffered, no local echo and non-canonicalized input */
		tcgetattr(0, &savedTerm);
		atexit(exit_handler);
		
		tcgetattr(0, &newTerm);
		newTerm.c_lflag &= ~(ECHO | ICANON);
		tcsetattr(0, TCSANOW, &newTerm);
		setvbuf(stdin, NULL, _IONBF, 0);
		
		/* git'er done! */
		bfvm_run(vm);
	}

	bfvm_free(vm);
	
	return rval;
}
