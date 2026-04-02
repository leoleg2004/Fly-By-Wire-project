/*
 * Main program
 */
#include "../lib/matrix_library.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {	
	if(argc < 3) {
		printf("\nWARNING: missing matrix size (rows, columns)\n");
		exit(1);
	}
	
	int rows = atoi(argv[1]);
	int cols  = atoi(argv[2]);

	t_matrix* mat = matrix_new(rows, cols);
	
	matrix_print(mat);
	
	return 0;
}