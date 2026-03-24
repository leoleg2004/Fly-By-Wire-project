/*
 * Main program
 */
#include "../lib/matrix_library.h"
#include "../lib/bug_library.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#define clrscr() printf("\e[1;1H\e[2J")

int main(int argc, char* argv[]) {	
	if(argc < 4) {
		printf("\nWARNING: missing matrix size (rows, columns) and bug lenght\n");
		exit(1);
	}
	
	int rows   = atoi(argv[1]);
	int cols    = atoi(argv[2]);
	int length = atoi(argv[3]);
	
	printf("\nnew matrix(%d, %d) bug length=%d", rows, cols, length);
	t_matrix* field = matrix_new(rows, cols);
	
	t_bug* bug = bug_new(field, length);
	
	bug_init(bug, 4, 4, 1);	
	clrscr();
	bug_dump(bug);
	matrix_print(field);

	// do-while loop to read input data  
	char direction;
	do { 
		scanf(" %c", &direction);
		switch(direction) {
			case '8' : bug_up(bug); break;
			case '2' : bug_down(bug); break;
			case '4' : bug_left(bug); break;
			case '6' : bug_right(bug); break;
			default : printf("null\n"); 
		}
		clrscr();
		// dump the bug on the field
		bug_dump(bug);
	
		matrix_print(field);
     } while (direction != '0');
	
	
	return 0;
}
