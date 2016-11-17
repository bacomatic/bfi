# bfi
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
