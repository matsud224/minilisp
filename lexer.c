#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "header.h"

#define SEEN_TOKEN_STACK_SIZE 20
Token seen_token_stack[SEEN_TOKEN_STACK_SIZE]; //先読みした後に押し戻されたトークンをためておくスタック(溢れないように最大押し戻し回数分確保)
int seen_token_count=0;


int ismacrochar(char c){
	return (c=='(') || (c==')') || (c=='\'') || (c==';') || (c=='"') || (c=='`') || (c=='.') || (c=='#');
}

Token token_get(FILE* fp){
	int i;
	char temp[IDENT_MAX_STR_LEN];
	int nextchar;
	Token restok;

	if(seen_token_count>0){
        return seen_token_stack[--seen_token_count];
	}

	nextchar=getc(fp);

	//スペース読みとばし
	while(isspace(nextchar)){
		nextchar=getc(fp);
	}

	if(isgraph(nextchar) && !isdigit(nextchar) && !ismacrochar(nextchar)){
		temp[0]=nextchar;

		for(i=1;i<IDENT_MAX_STR_LEN;i++){
			nextchar=getc(fp);
			if(isgraph(nextchar) && !ismacrochar(nextchar)){
				temp[i]=nextchar;
			}else{
				ungetc(nextchar,fp);
				break;
			}
		}
		temp[i]='\0';

		if((temp[0]=='-' || temp[0]=='+') && temp[1]!='\0'){
			goto gentoken_int;
		}

		restok.tag=TOK_SYMBOL;
		restok.value.symbol=sym_get(temp);
	}else if(isdigit(nextchar)){
		temp[0]=nextchar;
		for(i=1;i<IDENT_MAX_STR_LEN;i++){
			nextchar=getc(fp);
			if(isdigit(nextchar)){
				temp[i]=nextchar;
			}else{
				ungetc(nextchar,fp);
				break;
			}
		}
		temp[i]='\0';
gentoken_int:
		restok.tag=TOK_INTVAL;
		restok.value.intval=atoi(temp);
	}else if(nextchar==EOF){
		restok.tag=TOK_ENDOFFILE;
	}else{
		char subchar=getc(fp);
		if(nextchar=='#' && (subchar=='t' || subchar=='f')){
			temp[0]='#';temp[1]=subchar;temp[2]='\0';
			restok.tag=TOK_SYMBOL;
			restok.value.symbol=sym_get(temp);
		}else{
			ungetc(subchar,fp);
			restok.tag=TOK_ASCII;
			restok.value.ascii=nextchar;
		}
	}

	return restok;
}

void token_unget(Token t){
    if(seen_token_count==SEEN_TOKEN_STACK_SIZE){
        error("internal stack overflow.");
    }
    seen_token_stack[seen_token_count++]=t;
}
