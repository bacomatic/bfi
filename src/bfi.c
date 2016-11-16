/*
	Copyright (c) 2006, David DeHaven
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
		*	Redistributions of source code must retain the above copyright 
			notice, this list of conditions and the following disclaimer.
		*	Redistributions in binary form must reproduce the above copyright
			notice, this list of conditions and the following disclaimer in the
			documentation and/or other materials provided with the distribution.
		*	Neither the name of the author nor the names of its
			contributors may be used to endorse or promote products derived from
			this software without specific prior written permission.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/	

/*
	A Brainf*ck interpreter (not compiler!)
	
	Most comprehensive collection of information:
	http://en.wikipedia.org/wiki/Brainfuck
	
	Original author's site seems to be:
	http://www.muppetlabs.com/~breadbox/bf/
	
	hello.b:
	>+++++++++[<++++++++>-]<.>++++++[<+++++>-]<-.+++++++..+++.
	>>+++++++[<++++++>-]<++.------------.<++++++++.--------.
	+++.------.--------.>+.>++++++++++.
	
	Excerpted from the author's website:
	
									The Language
	
		A Brainfuck program has an implicit byte pointer, called "the pointer",
	which is free to move around within an array of 30000 bytes, initially all
	set to zero. The pointer itself is initialized to point to the beginning of
	this array.

		The Brainfuck programming language consists of eight commands, each of
	which is represented as a single character.
	
	> - Increment the pointer.
	< - Decrement the pointer.
	+ - Increment the byte at the pointer.
	- - Decrement the byte at the pointer.
	. - Output the byte at the pointer.
	, - Input a byte and store it in the byte at the pointer.
	[ - Jump forward past the matching ] if the byte at the pointer is zero.
	] - Jump backward to the matching [ unless the byte at the pointer is zero.
	
	[] loops can be nested
	
	Taken directly from:
	http://www.muppetlabs.com/~breadbox/bf/standards.html
	
			The Unofficial Constraints on Portable Brainfuck Implementations

	-	The actual size of the cell array is implementation-defined. However,
		the array shall always contain at least 9999 cells. (Allowing the size
		to be as small as a 4-digit number is done for the benefit of
		programmers writing interpreters in three lines of C code and the like.)
	-	If a program attempts to move the pointer below the first array cell, or
		beyond the last array cell, then that program's behavior is undefined.
		(A few implementations cause the pointer to wrap around, but many,
		perhaps most, implementations behave in a manner consistent with a C
		pointer wandering off into arbitrary memory.)
	-	The range of values a single cell can contain is implementation-defined.
		(The range need not be consistent, either: consider the case of a
		"bignum" implementation, whose cells' ranges would be limited only by
		currently available resources.) However, the range of every cell shall
		always at least include the values 0 through 127, inclusive.
	-	If a program attempts to either decrement the value of a cell below its
		documented minimum value, if any, or increment the value of a cell
		beyond its documented maximum value, if any, then the value in the cell
		after such an operation is implementation-defined. (Most implementations
		choose to let the value wrap around in a fashion typical to C integers,
		but this is not required.)
	-	If a program attempts to input a value when there is no more data in the
		input stream, the value in the current cell after such an operation is
		implementation-defined. (The most common choices are to either store 0,
		or store -1, or to leave the cell's value unchanged. This is frequently
		the most problematic issue for the programmer wishing to achieve
		portability.)
	-	If a program contains one or more unbalanced brackets, then the behavior
		of that program is undefined. (In fact, a number of Brainfuck compilers
		will crash during compilation itself.) (And no, I'm not going to
		formally describe what "unbalanced" means here. You all know what it
		means.)
	
	This interpreters conformance to the above guidelines:
		1> The cell array is fixed at 30000 cells
		2> The cell pointer will wrap around when moved past the ends
		3> Each cell is an 8 bit unsigned integer, range [0..255]
		4> Cell values wrap when incremented or decremented beyond that range
		5> Platform dependent; on Mac OS X, input is allowed until the program
		   terminates. When a ^D is sent, a zero value is returned to the
		   program, if a file is the source of input, additional reads will
		   return zeros
		6> Yep, pretty much undefined. Good luck!
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <termios.h>


/* Brainfuck VM */
unsigned char *sourcePool = NULL;
long sourceSize = 0;
long Sp = 0;

unsigned char theArray[30000];
int thePointer = 0;

struct termios savedTerm;

void exitHandler()
{
	/* reset terminal settings */
	tcsetattr(0, TCSANOW, &savedTerm);
}

void run()
{
	memset(theArray, 0, 30000);
	thePointer = 0;
	
	Sp = 0;
	
	while(Sp < sourceSize) {
		int temp;
		
		switch(sourcePool[Sp]) {
			case '>':
				thePointer++;
				if(thePointer >= 30000) thePointer = 0;
				break;
			
			case '<':
				if(thePointer == 0) thePointer = 30000;
				thePointer--;
				break;
			
			case '+':
				theArray[thePointer]++;
				break;
			
			case '-':
				theArray[thePointer]--;
				break;
			
			case '.':
				putchar((int)theArray[thePointer]);
				break;
			
			case ',':
				temp = getchar();
				if(temp != 0x04) {	/* ^D sends 0x04 (EOT) in the mode we're running in */
					theArray[thePointer] = (unsigned char)temp;
				} else {
					theArray[thePointer] = 0;
				}
				putchar(theArray[thePointer]);
				fflush(stdout);
				break;
			
			case '[':
				temp = 1; /* nesting level */
				if(theArray[thePointer] == 0) {
					Sp++;
					while(Sp < sourceSize) {
						if(sourcePool[Sp] == ']') {
							temp--;
							if(temp == 0) break; /* found matching brace */
						} else if(sourcePool[Sp] == '[') {
							temp++;
						}
						Sp++;
					}
				}
				break;
			
			case ']':
				temp = 1;
				if(theArray[thePointer] != 0) {
					Sp--;
					while(Sp > 0) {
						if(sourcePool[Sp] == '[') {
							temp--;
							if(temp == 0) break; /* found matching brace */
						} else if(sourcePool[Sp] == ']')
							temp++;
						Sp--;
					}
				}
				break;
			
			default:
				break;
		}
		
		Sp++;
	}
}

/*
	We take one argument, the program file to load and execute
*/
int main(int argc, char **argv)
{
	struct termios newTerm;
	int rval = 0;
	FILE *fp = NULL;
	
	if(argc != 2) {
		fprintf(stderr, "Usage: %s <brainfuck file>\n", argv[0]);
		return -1;
	}
	
	/* set term settings to unbuffered, no local echo and non-canonicalized input */
	tcgetattr(0, &savedTerm);
	atexit(exitHandler);
	
	tcgetattr(0, &newTerm);
	newTerm.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(0, TCSANOW, &newTerm);
	setvbuf(stdin, NULL, _IONBF, 0);
	
	/* git'er done! */
	fp = fopen(argv[1],"r");
	if(fp) {
		struct stat fps;
		
		if(fstat(fileno(fp), &fps) == 0) {
			sourceSize = (size_t)fps.st_size;
			sourcePool = (unsigned char*)malloc(sourceSize);
			
			if(fread(sourcePool, sourceSize, 1, fp) != 1) {
				fprintf(stderr, "Error reading from file %s\n", argv[1]);
				rval = -1;
			} else {
				run();
			}
		} else {
			fprintf(stderr, "Error 'stat'ing file %s: %d - %s\n", argv[1], errno, strerror(errno));
			rval = -1;
		}
		fclose(fp);
	} else {
		fprintf(stderr, "Unable to open file %s\n", argv[1]);
		rval = -1;
	}
	
	if(sourcePool) free(sourcePool);
	
	return 0;
}
