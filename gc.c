//Mark Sweep GC
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "header.h"
#define INIT_NUM_OF_CONS 5000
#define EXPAND_NUM_OF_CONS 50

Cons* fromspace;
Cons* tospace;

GCStack GCSTACK;

int num_of_cons = 0;
int next_freecons=0;

void gc_expand();


int entry_count(GCEntry* ptr){
	int count=0;
	while(ptr){
		count++;
		ptr=ptr->next;
	}
	return count;
}

int stack_count(GCStack* ptr){
	int count=0;
	while(ptr){
		count++;
		ptr=ptr->next;
	}
	return count;
}

void gc_init(){
	num_of_cons=INIT_NUM_OF_CONS;
	next_freecons=0; //0番地はnil用
	fromspace=(Cons*)malloc(sizeof(Cons)*num_of_cons);
	tospace=(Cons*)malloc(sizeof(Cons)*num_of_cons);
	if(!fromspace || !tospace){error("Allocation failed!");}

	GCConsPtr nil=gc_getcons();
	DEREF(nil).car.tag=TAG_NIL;
	DEREF(nil).car.value=0;
	DEREF(nil).cdr.tag=TAG_NIL;
	DEREF(nil).cdr.value=0;

	GCSTACK.next=NULL;
}

void gcstack_push(){
	GCStack* beforetop=GCSTACK.next;
	GCSTACK.next=malloc(sizeof(GCStack));
	GCSTACK.next->next=beforetop;
	GCSTACK.next->entry.next=NULL;
}

void gcstack_pop(){
	if(GCSTACK.next==NULL){return;}
	GCStack* second=GCSTACK.next->next;
	GCEntry* eptr=GCSTACK.next->entry.next;
	while(eptr){
		GCEntry* temp=eptr->next;
		free(eptr);
		eptr=temp;
	}
	free(GCSTACK.next);
	GCSTACK.next=second;
	return;
}

//変数をrootに入れる(引数の最後はNULLで終わらせないといけない)
void gc_watchvar_ptr(GCConsPtr* var,...){
	if(GCSTACK.next==NULL){error("GCSTACK is empty.");}
	va_list args;
	va_start(args,var);
	GCConsPtr* current=var;

	while(current){
		GCEntry* newentry=malloc(sizeof(GCEntry));
		newentry->use_tag=0;
		newentry->ptr=current;
		newentry->next=GCSTACK.next->entry.next;
		GCSTACK.next->entry.next=newentry;

		current=va_arg(args,GCConsPtr*);
	}
	va_end(args);
	return;
}

void gc_watchvar_pair(TagValuePair* var,...){
	if(GCSTACK.next==NULL){error("GCSTACK is empty.");}
	va_list args;
	va_start(args,var);
	TagValuePair* current=var;
	while(current){
		GCEntry* newentry=malloc(sizeof(GCEntry));
		newentry->use_tag=1;
		newentry->ptr=&(current->value);
		newentry->tag=&(current->tag);
		newentry->next=GCSTACK.next->entry.next;
		GCSTACK.next->entry.next=newentry;

		current=va_arg(args,TagValuePair*);
	}

	va_end(args);
	return;
}

GCConsPtr gc_getcons(){
	if(next_freecons<num_of_cons){
		return next_freecons++;
	}

	gc_copy();

	if(next_freecons>=num_of_cons){
		gc_expand();
	}

	return next_freecons++;
}

GCConsPtr gc_getcons_seq(int n){
retry:
	if(next_freecons+n-1<num_of_cons){
		int before=next_freecons;
		before+=n;
		return before;
	}

	gc_copy();

	if(next_freecons+n-1>=num_of_cons){
		gc_expand();
	}

	goto retry;
}

//領域サイズを拡張
void gc_expand(){
	num_of_cons+=EXPAND_NUM_OF_CONS;

	fromspace=(Cons*)realloc(fromspace,sizeof(Cons)*num_of_cons);
	free(tospace);
	tospace=(Cons*)malloc(sizeof(Cons)*num_of_cons);

	if(!fromspace || !tospace){error("Allocation failed.");}

	return;
}



void gc_copy(){
	int scanned=0,unscanned=0;

    //rootからコピー
    GCStack* sptr;
    GCEntry* eptr;

	//nil(0番地)をコピー
	tospace[0].car.tag=TAG_NIL;
	tospace[0].car.value=0;
	tospace[0].cdr.tag=TAG_NIL;
	tospace[0].cdr.value=0;
	scanned=1;
	unscanned=1;

    for(sptr=GCSTACK.next;sptr;sptr=sptr->next){
		for(eptr=sptr->entry.next;eptr;eptr=eptr->next){
			if(*eptr->ptr==UNUSED){
				continue;
			}

			if(eptr->use_tag==1 && ((*eptr->tag)!=TAG_PTR)){
				continue;
			}

			//*より->のほうが優先順位が高い
			if(fromspace[*eptr->ptr].car.tag==TAG_BROKENHEART){
				*eptr->ptr=fromspace[*eptr->ptr].car.value;
			}else{
				tospace[unscanned]=fromspace[*eptr->ptr];
				fromspace[*eptr->ptr].car.tag=TAG_BROKENHEART;
				fromspace[*eptr->ptr].car.value=unscanned;
				*eptr->ptr=unscanned;
				unscanned++;
			}
		}
    }

    //子オブジェクトをコピー
    while(scanned<unscanned){
    	int child;
    	if(tospace[scanned].car.tag==TAG_PTR){
    		child=tospace[scanned].car.value;
    		if(fromspace[child].car.tag==TAG_BROKENHEART){
    			tospace[scanned].car.value=fromspace[child].car.value;
    		}else{
    			tospace[unscanned]=fromspace[child];
    			tospace[scanned].car.value=unscanned;
    			fromspace[child].car.tag=TAG_BROKENHEART;
				fromspace[child].car.value=unscanned;
    			unscanned++;
    		}
		}
		if(tospace[scanned].cdr.tag==TAG_PTR){
    		child=tospace[scanned].cdr.value;
    		if(fromspace[child].car.tag==TAG_BROKENHEART){
    			tospace[scanned].cdr.value=fromspace[child].car.value;
    		}else{
    			tospace[unscanned]=fromspace[child];
    			tospace[scanned].cdr.value=unscanned;
    			fromspace[child].car.tag=TAG_BROKENHEART;
				fromspace[child].car.value=unscanned;
    			unscanned++;
    		}
		}

    	scanned++;
    }

    Cons* temp=fromspace;
    fromspace=tospace;
    tospace=temp;

    next_freecons=unscanned;

    return;
}
