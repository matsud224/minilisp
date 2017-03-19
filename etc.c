#include <stdio.h>
#include <string.h>
#include "header.h"


TagValuePair NIL={TAG_NIL,0};
TagValuePair T={TAG_T};
TagValuePair F={TAG_F};
TagValuePair TERMINAL={TAG_UNUSED,UNUSED};

TagValuePair INTCONST(int v){
	TagValuePair tgp={TAG_INT,v};
	return tgp;
}

TagValuePair SYMCONST(char* str){
	TagValuePair tgp={TAG_SYM,sym_get(str)};
	return tgp;
}

TagValuePair SYMLIST(char* str){
	return list(SYMCONST(str),NULL);
}

TagValuePair INST(int name,TagValuePair arg){
	TagValuePair instname={TAG_INST,name};
	return list(cons(instname,arg),NULL);
}
