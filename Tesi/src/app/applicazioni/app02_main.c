#include <stdio.h>
#include <stdlib.h>

void printArguments(int size, char* arguments[]);


int main(int argc, char* argv[]) {
	
	printArguments(argc, argv);
			
    exit(0);
}


void printArguments(int size, char* arguments[]) {
	printf("\n");
	
	for(int i=0; i < size; i++) {
		
		printf(  "argument[%d] : %s\n",   i  ,   arguments[i]   );
    
	}
	
	printf("\n\n");
}


