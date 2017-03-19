#include <stdio.h>
#include <malloc.h>
#include "header.h"


int main(int argc,char* argv[]){
	gc_init();
	gcstack_push();

	evaluator_init();

    while(1){
		printf("> ");
		TagValuePair readexpr=read(stdin);
        print(compile(readexpr));
		printf("\n---------\n");
		print(eval(readexpr));
		printf("\n");
	}

	gcstack_pop();

	return 0;
}
