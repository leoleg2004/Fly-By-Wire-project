/*
 * print_library.c
 */
#include "print_library.h"
#include <stdio.h>

void print_arguments(int size, char* arguments[]) {
	printf("\n");
	
	for(int i=0; i < size; i++) {
		
		printf(  "                                    argument[%d] : %s\n",   i  ,   arguments[i]   );
    
	}
	
	printf("\n\n");
}


