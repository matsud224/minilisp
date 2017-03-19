#include <stdio.h>
#include <string.h>
#include "header.h"

TagValuePair read_list(FILE* fp);


TagValuePair read(FILE* fp){
	gcstack_push();

    TagValuePair ret={TAG_UNUSED};
    Token gottoken=token_get(fp);
	gc_watchvar_pair(&ret,NULL);

    switch(gottoken.tag){
	case TOK_INTVAL:
		ret.tag=TAG_INT;
		ret.value=gottoken.value.intval;
		break;
	case TOK_SYMBOL:
		if(strcmp("nil",sym_ref(gottoken.value.symbol))==0){
			ret.tag=TAG_NIL;
		}else if(strcmp("#t",sym_ref(gottoken.value.symbol))==0){
			ret.tag=TAG_T;
		}else if(strcmp("#f",sym_ref(gottoken.value.symbol))==0){
			ret.tag=TAG_F;
		}else{
			ret.tag=TAG_SYM;
			ret.value=gottoken.value.symbol;
		}
		break;
	case TOK_ASCII:
		switch(gottoken.value.ascii){
			case '(':
				ret=expand_macro(read_list(fp));

				break;
			case '\'':
				ret=list(SYMCONST("quote"),read(fp),NULL);
				break;
			default:
				error("Unexpected token.");
				break;
		}
		break;
	case TOK_ENDOFFILE:
		break;
	default:
		error("Unexpected token.");
		break;
    }

	gcstack_pop();
    return ret;
}

TagValuePair read_list(FILE* fp){
	gcstack_push();

	TagValuePair ret={TAG_UNUSED};
	GCConsPtr cons=UNUSED,nextcons=UNUSED;
	Token gottoken;
	gc_watchvar_pair(&ret,NULL);
	gc_watchvar_ptr(&cons,&nextcons,NULL);

	gottoken=token_get(fp);
	if(gottoken.tag==TOK_ASCII && gottoken.value.ascii==')'){
		ret.tag=TAG_NIL;
		gcstack_pop();
		return ret;
	}else{
		token_unget(gottoken);
	}

	cons=gc_getcons();
	ret.tag=TAG_PTR;
	ret.value=cons;

	while(1){
		DEREF(cons).car=read(fp);

		gottoken=token_get(fp);
		if(gottoken.tag==TOK_ASCII && gottoken.value.ascii==')'){
			DEREF(cons).cdr.tag=TAG_NIL;
			break;
		}else if(gottoken.tag==TOK_ASCII && gottoken.value.ascii=='.'){
			DEREF(cons).cdr=read(fp);
			gottoken=token_get(fp);
			if(!(gottoken.tag==TOK_ASCII && gottoken.value.ascii==')')){
				error("More than one object follows . in list.");
			}
			break;
		}else{
			token_unget(gottoken);
			nextcons=gc_getcons();
			DEREF(cons).cdr.tag=TAG_PTR;
			DEREF(cons).cdr.value=nextcons;
			cons=nextcons;
		}
	}

	gcstack_pop();
	return ret;
}
