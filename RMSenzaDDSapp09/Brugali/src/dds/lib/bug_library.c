/*
 * bug_library.c
 */
#include "matrix_library.h"
#include "bug_library.h"
#include <stdio.h>
#include <stdlib.h>


// allocate the memory for a bug of length l for matrix field
t_bug* bug_new(t_matrix* field, int l) {
	// dynamically allocate the memory for the bug data structure
	t_bug* b;
	b = (t_bug*) malloc(sizeof(t_bug));
	b->body = (int*) malloc(l*sizeof(int));
	b->field = field;
    b->length = l;
	return b;
}

// init the body of the bug
void bug_init(t_bug* bug, int head_r, int head_c, int dir) {	
	// check the initial direction of the bug
	int delta_r, delta_c;
	if(dir == 0) {			// heading up
		delta_r = +1;
		delta_c = 0;
		printf("\nheading up\n");
	}
	else if(dir == 1) {	// heading right
		delta_r = 0;
		delta_c = -1;
		printf("\nheading right\n");
	}
	else if(dir == 2) {	// heading down
		delta_r = -1;
		delta_c = 0;
		printf("\nheading down\n");
	}
	else if(dir == 3) {	// heading left
		delta_r = 0;
		delta_c = 1;
		printf("\nheading left\n");
	}
	else {
		printf("\nERROR : invalid bud direction\n");
		return;
	}
	
	// store the index of the cell of the matrix corresponding to the head of the bug
	bug->body[0] = head_r * bug->field->cols + head_c;	

	int body_r = head_r;
	int body_c = head_c;
	for(int i=1; i < bug->length; i++) {
		body_r += delta_r;
		body_c += delta_c;
		// store the index of the cell of the matrix corresponding to the body of the bug
		bug->body[i] = body_r * bug->field->cols + body_c;	
	}		
}

void bug_shift(t_bug* bug){
	int pos;
	
	// the cell occupied by the tail of the bug is overwritten with "*"
	pos = bug->body[ bug->length-1];
	((char*)bug->field->data)[pos] = '*';
	
	// shift the body of the bug of one cell
	for(int i=bug->length-1; i >  0; i--) {
		//printf("b[%d] = %d\n", i, bug->body[i]);
		bug->body[i] = bug->body[i-1];
	}
}


void bug_up(t_bug* bug) {
	// get the cell occupied by the head
	int r, c;
	matrix_cell(bug->field, bug->body[0], &r, &c);
	if(r == 0)
		return; // the bug reached the top border

	// shift the bug body
	bug_shift(bug);
		
	// move up the head of the bug
	r--;
	bug->body[0] =  matrix_index(bug->field, r, c);
}

void bug_down(t_bug* bug) {
	// get the cell occupied by the head
	int r, c;
	matrix_cell(bug->field, bug->body[0], &r, &c);
	if(r == bug->field->rows-1)
		return; // the bug reached the bottom border

	// shift the bug body
	bug_shift(bug);
		
	// move down the head of the bug
	r++;
	bug->body[0] =  matrix_index(bug->field, r, c);
}
	
void bug_left(t_bug* bug) {
	// get the cell occupied by the head
	int r, c;
	matrix_cell(bug->field, bug->body[0], &r, &c);
	if(c == 0)
		return; // the bug reached the left border

	// shift the bug body
	bug_shift(bug);
		
	// move left the head of the bug
	c--;
	bug->body[0] =  matrix_index(bug->field, r, c);	
}

void bug_right(t_bug* bug) {
	// get the cell occupied by the head
	int r, c;
	matrix_cell(bug->field, bug->body[0], &r, &c);
	if(c == bug->field->cols-1)
		return; // the bug reached the bottom border

	// shift the bug body
	bug_shift(bug);
	
	// move right the head of the bug
	c++;
	bug->body[0] =  matrix_index(bug->field, r, c);	
}	


void bug_dump(t_bug* bug) {
	((char*)bug->field->data)[bug->body[0]] = 'o';
	for(int i=1; i < bug->length; i++)
		((char*)bug->field->data)[bug->body[i]] = 'x';
}

