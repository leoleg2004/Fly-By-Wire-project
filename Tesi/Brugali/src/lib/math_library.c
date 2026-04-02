/*
 * math_library.c
 */
#include "math_library.h"
#include <stdio.h>

/*
 * Read two integer numbers from the command line and print the sum
 */
int math_sum(int a[], int size) {
	int sum = 0;
	
	for(int i = 0; i < size; i++)
		sum += a[i];
	
	return sum;
}

int math_max(int a[], int size) {
	int max = 0;
	
	for(int i = 0; i < size; i++)
		if(a[i] > max)
			max = a[i];
	
	return max;
}
