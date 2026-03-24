/*
 * bug_library.h
 */
#ifndef BUG_LIBRARY
#define BUG_LIBRARY

#include "matrix_library.h"

// the bug data structure
typedef struct bug {
	int* body;
	int length;
	t_matrix* field;
} t_bug;

// allocate the memory for a bug of length l for matrix field
t_bug* bug_new(t_matrix* field, int l);

// init the bug position
void bug_init(t_bug* bug, int r, int c, int dir);

// shift forward the bug position 
void bug_shift(t_bug* bug);

// move the head of the bug
void bug_up(t_bug* bug);
void bug_down(t_bug* bug);
void bug_left(t_bug* bug);
void bug_right(t_bug* bug);

// dump the bug on the field
void bug_dump(t_bug* bug);

#endif