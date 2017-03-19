#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "header.h"

void print_list(int index);

void print(TagValuePair pair){
	gcstack_push();

	gc_watchvar_pair(&pair,NULL);

    switch(pair.tag){
		case TAG_INT:
			printf("%d",pair.value);
			break;
		case TAG_CHAR:
			printf("%c",(char)pair.value);
			break;
		case TAG_NIL:
			printf("nil");
			break;
		case TAG_SYM:
			printf("%s",sym_ref(pair.value));
			break;
		case TAG_PTR:
			printf("(");
			print_list(pair.value);
			printf(")");
			break;
		case TAG_CLOSURE:
			printf("#<closure>");
			break;
		case TAG_SUBR:
			printf("#<subr>");
			break;
		case TAG_CONTINUATION:
			printf("#<continuation>");
			break;
		case TAG_T:
			printf("#t");
			break;
		case TAG_F:
			printf("#f");
			break;
		case TAG_INST:
			switch(pair.value){
				case INST_NIL:
					printf("NIL");
					break;
				case INST_NOP:
					printf("NOP");
					break;
				case INST_REF:
					printf("REF");
					break;
				case INST_TLVAL:
					printf("TLVAL");
					break;
				case INST_PUSH:
					printf("PUSH");
					break;
				case INST_PUSHCLOSURE:
					printf("PUSHCLOSURE");
					break;
				case INST_TLDEF:
					printf("TLDEF");
					break;
				case INST_POP:
					printf("POP");
					break;
				case INST_APPLY:
					printf("APPLY");
					break;
				case INST_BNIL:
					printf("BNIL");
					break;
				case INST_PUSHFLAME:
					printf("PUSHFLAME");
					break;
				case INST_PARAMBIND:
					printf("PARAMBIND");
					break;
				case INST_STORE:
					printf("STORE");
					break;
				case INST_TLVALSTORE:
					printf("TLVALSTORE");
					break;
				case INST_DUP:
					printf("DUP");
					break;
				case INST_MACDEF:
					printf("MACDEF");
					break;
			}
			break;
		case TAG_UNUSED:
			printf("No value.");
			break;
		default:
			error("Bad tag.");
	}

	fflush(stdout);
	gcstack_pop();
    return;
}

void print_list(GCConsPtr index){
	gcstack_push();

	GCConsPtr current=index;
	gc_watchvar_ptr(&index,NULL);

	while(1){
		print(DEREF(current).car);
		if(DEREF(current).cdr.tag!=TAG_NIL){
			if(DEREF(current).cdr.tag==TAG_PTR){
				printf(" ");
				current=DEREF(current).cdr.value;
			}else{
				printf(" . ");
				print(DEREF(current).cdr);
				break;
			}
		}else{
			break;
		}
	}

	gcstack_pop();
	return;
}
