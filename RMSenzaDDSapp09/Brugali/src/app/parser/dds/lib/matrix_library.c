/*
 * matrix_library.c
 */
#include "matrix_library.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

t_matrix* matrix_new(int r, int c) {
	// dynamically allocate the memory for the matrix data structure
	t_matrix* m;
	m = (t_matrix*) malloc(sizeof(t_matrix));
	
	// dynamically allocate the memory for the array of char
	m->data = (char*) malloc( r * c * sizeof(char));
	m->rows = r;
	m->cols = c;
	
	// init the matrix cells with
	for(int i=0; i < r*c; i++)
		m->data[i] = '-';
	
	return m;
}


void matrix_print(t_matrix* m) {
	if(m == 0)
		printf("Error! Invalid matrix\n");
	
	for(int r=0; r < m->rows; r++) {
		// print a row
		for(int c=0; c < m->cols; c++) {
			printf("%c ", m->data[r*m->cols+c]);
		}
		// new row
		printf("\n");
	}
}

int matrix_index(t_matrix* m, int r, int c) {
	return r*m->cols+c;
}

void matrix_cell(t_matrix* m, int index, int *r, int *c) {
	*r  = index / m->cols;
	*c = index - (*r) * m->cols;
}
