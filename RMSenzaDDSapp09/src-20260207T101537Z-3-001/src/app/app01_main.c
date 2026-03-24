#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
	
	printf("\n");
	
	for(int i=0; i < argc; i++) {
		
		printf(  "parameter[%d] : %s\n",   i  ,   argv[i]   );
    
	}
	
	printf("\n\n");
			
    exit(0);   
}


