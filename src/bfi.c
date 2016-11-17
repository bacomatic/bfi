/*
	Copyright (c) 2006, 2016, David DeHaven
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
