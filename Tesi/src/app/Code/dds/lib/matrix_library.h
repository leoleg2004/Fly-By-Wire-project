/*
 * matrix_library.h
 */
#ifndef MATRIX_LIBRARY
#define MATRIX_LIBRARY

// the matrix data structure
typedef struct matrix {
	char* data;
	int rows;
	int cols;
} t_matrix;

// allocate the memory for a matrix with r rows and c columns
t_matrix* matrix_new(int r, int c);

// print the content of the matrix
void matrix_print(t_matrix* m);

int matrix_index(t_matrix* m, int r, int c);
void matrix_cell(t_matrix* m, int index, int *r, int *c);

#endif