/*
 * Main program
 */
#include "../lib/math_library.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {	
	/*
	 * check the input parameters
	 */
	int operation = 0;	// selection of the mathematical operation
	
	if(argc < 2) {
		printf("\nWARNING: missing parameter\n");
		exit(1);
	}
	
	if(strcmp(argv[1], "sum") == 0)
		operation = 1;
	else if(strcmp(argv[1], "max") == 0)
		operation = 2;
	else {
		printf("\nWARNING: wrong operation\n\n");
		exit(1);
	}

	/*
	 * get the input data
	 */
	int count = 0;  	// index where the element is to be inserted
    	int a[100];		// array of integer numbers
	int number;		// a variable to hold the input number
	
	printf("\nDigit a sequence of space-separated integers. Press Enter to stop.\nnumbers: ");
    
	// do-while loop to read input data
    	do { 
        	scanf("%d", &number);
		a[count] = number; // Take input at position count and increment count
		count++;
 
        	// If '\n' (newline) has occurred or the whole array is filled,
	        // then exit the loop
        	// Otherwise, continue
	} while (getchar() != '\n' && count < 100);
 
	/*
	 * call the selected function
	 */
	switch(operation) {
	case 1 :
		int sum = math_sum(a, count);
		printf("\nsum = %d\n\n", sum);
		break;
    	case 2:
		int max = math_max(a, count);
		printf("\nmax = %d\n\n", max);
		break;
	}
	exit(0);
}


				 
