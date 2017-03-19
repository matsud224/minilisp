#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "header.h"


SymbolTable symtable[SYMTABLE_LEN]={{NULL,NULL,{TAG_UNUSED,UNUSED},{TAG_UNUSED,UNUSED}}};
int anonvar_id=0;

int sym_hash(char* str){
	char* curr=str;
	int value=0;
	while(*curr!='\0'){
		value+=*curr;
		curr++;
	}
	return value%SYMTABLE_LEN;
}
/*
TagValuePair str_to_charlist(char* str){
	gcstack_push();

	TagValuePair ret;
	char* ptr=str;

	if(*ptr=='\0'){
		ret.tag=TAG_NIL;
		gcstack_pop();
		return ret;
	}

	GCConsPtr cons=gc_getcons();
	ret.tag=TAG_PTR;
	ret.value=cons;

	while(1){
		DEREF(cons).car.tag=TAG_CHAR;
		DEREF(cons).car.value=*ptr;

		ptr++;
		if(*ptr=='\0'){
			DEREF(cons).cdr.tag=TAG_NIL;
			break;
		}else{
			GCConsPtr nextcons=gc_getcons();
			DEREF(cons).cdr.tag=TAG_PTR;
			DEREF(cons).cdr.value=nextcons;
			cons=nextcons;
		}
	}

	gcstack_pop();
	return ret;
}*/
/*
char* charlist_to_str(TagValuePair pair){
	if(pair.tag!=TAG_PTR){
		error("Charcter list expected.");
	}
	GCConsPtr cons=pair.value;
	int current_len=5;
	int current_pos=0;
	char* result=malloc(sizeof(char)*current_len);
	char* temp;

	if(result==NULL){error("Allocation failed.");}

	while(1){
		if(cons.car.tag!=TAG_CHAR){
			error("Charcter list expected.");
		}else{
			if(current_len<=current_pos){
				current_len+=5;
				temp=realloc(result,sizeof(char)*current_len);
				if(temp==NULL){error("Allocation failed.");}
				result=temp;
			}
			result[current_pos]=cons.car.value;
			current_pos++;
		}
		if(cons.cdr.tag!=TAG_PTR){
			error("Charcter list expected.");
		}else if(cons.cdr.tag==TAG_NIL){
			if(current_len<=current_pos){
				current_len+=5;
				temp=realloc(result,sizeof(char)*current_len);
				if(temp==NULL){error("Allocation failed.");}
				result=temp;
			}
			result[current_pos]='\0';
			current_pos++;
		}else{
			cons=INDEX(cons.cdr.value);
		}
	}

	temp=realloc(result,current_pos);
	if(temp==NULL){error("Allocation failed.");}
	result=temp;
	return result;
}*/

int sym_get(char* str){
	//検索し、ポインタを返す。なければ登録も行う
	int shash=sym_hash(str);
	SymbolTable* searchptr=&symtable[shash];
	SymbolPtr symp;
	symp.part.hash=shash;
	symp.part.pos=0;
	while(searchptr->next!=NULL){
		if(strcmp(searchptr->next->name,str)==0){
			return symp.value;
		}else{
			searchptr=searchptr->next;
			symp.part.pos++;
		}
	}
	//見つからなかった...
	searchptr->next=malloc(sizeof(SymbolTable));
	searchptr->next->next=NULL;
	searchptr->next->name=malloc(sizeof(char)*(strlen(str)+1));
	searchptr->next->toplevel_val.tag=TAG_UNUSED;
	searchptr->next->toplevel_val.value=UNUSED;
	searchptr->next->expander.tag=TAG_UNUSED;
	searchptr->next->expander.value=UNUSED;
	strcpy(searchptr->next->name,str);

	return symp.value;
}

char* sym_ref(int ptr){
	SymbolPtr symp;
	symp.value=ptr;
	SymbolTable* searchptr=&symtable[symp.part.hash];

	int i;
	for(i=0;i<symp.part.pos;i++){
		searchptr=searchptr->next;
	}

	return searchptr->next->name;
}

TagValuePair sym_tlval(int ptr){
	SymbolPtr symp;
	symp.value=ptr;
	SymbolTable* searchptr=&symtable[symp.part.hash];

	int i;
	for(i=0;i<symp.part.pos;i++){
		searchptr=searchptr->next;
	}

	return searchptr->next->toplevel_val;
}

SymbolTable* sym_gettable(int ptr){
	SymbolPtr symp;
	symp.value=ptr;
	SymbolTable* searchptr=&symtable[symp.part.hash];

	int i;
	for(i=0;i<symp.part.pos;i++){
		searchptr=searchptr->next;
	}

	return searchptr->next;
}

int sym_get_anonymousvar(){
	char str[256];
	sprintf(str,"%%anonymous%d",anonvar_id);
	anonvar_id++;
	return sym_get(str);
}

int sym_get_labelvar(){
	char str[256];
	sprintf(str,"label-%d",anonvar_id);
	anonvar_id++;
	return sym_get(str);
}
