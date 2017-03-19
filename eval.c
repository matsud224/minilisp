#include "header.h"
#include <stdarg.h>

TagValuePair compile_list(TagValuePair env,TagValuePair expr);
TagValuePair compile_arguments(TagValuePair env,TagValuePair args);
TagValuePair compile_arguments_noeval(TagValuePair args);
TagValuePair compile_if(TagValuePair env,TagValuePair expr);

TagValuePair STACK;
TagValuePair ENVIRONMENT;
TagValuePair CODE;
TagValuePair DUMP;


//---事前にインターンしておくシンボル---
TagValuePair SYM_if;
TagValuePair SYM_quote;
TagValuePair SYM_lambda;
TagValuePair SYM_define;
TagValuePair SYM_begin;
TagValuePair SYM_set;
TagValuePair SYM_defmacro;
TagValuePair SYM_defun;
TagValuePair SYM_quasiquote;
TagValuePair SYM_unquote;
TagValuePair SYM_unquote_splicing;
//-----------------------------

void constsym_init(){
	SYM_if=SYMCONST("if");
	SYM_quote=SYMCONST("quote");
	SYM_lambda=SYMCONST("lambda");
	SYM_define=SYMCONST("define");
	SYM_begin=SYMCONST("begin");
	SYM_set=SYMCONST("set!");
	SYM_defmacro=SYMCONST("defmacro");
	SYM_defun=SYMCONST("defun");
	SYM_quasiquote=SYMCONST("quasiquote");
	SYM_unquote=SYMCONST("unquote");
	SYM_unquote_splicing=SYMCONST("unquote-splicing");
}

void env_init(){
	ENVIRONMENT=NIL;
	SETSUBR("car",SUBR_CAR);
	SETSUBR("cdr",SUBR_CDR);
	SETSUBR("cons",SUBR_CONS);
	SETSUBR("atom?",SUBR_ATOM);
	SETSUBR("eq?",SUBR_EQ);
	SETSUBR("+",SUBR_ADD);
	SETSUBR("*",SUBR_MUL);
	SETSUBR("-",SUBR_SUB);
	SETSUBR("div",SUBR_DIV);
	SETSUBR("mod",SUBR_MOD);
	SETSUBR("call/cc",SUBR_CALLCC);
	SETSUBR("eval",SUBR_EVAL);
}

void evaluator_init(){
	constsym_init();
	env_init();
	CODE=NIL;
	DUMP=NIL;
}


TagValuePair cons(TagValuePair car,TagValuePair cdr){
	gcstack_push();

	TagValuePair pair={TAG_UNUSED};
	GCConsPtr cons=UNUSED;
	gc_watchvar_pair(&car,&cdr,&pair,&cons,NULL);

	cons=gc_getcons();
	DEREF(cons).car=car;
	DEREF(cons).cdr=cdr;

	pair.tag=TAG_PTR;
	pair.value=cons;

	gcstack_pop();
	return pair;
}

TagValuePair list(TagValuePair p,...){
	gcstack_push();

	va_list args;
	va_start(args,p);
	TagValuePair current=p;
	TagValuePair result={TAG_UNUSED};
	GCConsPtr joint=gc_getcons();
	GCConsPtr tempjoint;
	result.tag=TAG_PTR;
	result.value=joint;

	while(current.tag!=TAG_UNUSED){
		DEREF(joint).car=current;
		current=va_arg(args,TagValuePair);
		if(current.tag==TAG_UNUSED){
			DEREF(joint).cdr=NIL;
		}else{
			tempjoint=gc_getcons();
			DEREF(joint).cdr.tag=TAG_PTR;
			DEREF(joint).cdr.value=tempjoint;
			joint=tempjoint;
		}
	}

	va_end(args);

	gcstack_pop();
	return result;
}

int length(TagValuePair lst){
	int count=0;
	while(lst.tag!=TAG_NIL){
		count++;
		lst=cdr(lst);
	}
	return count;
}

TagValuePair nconc(TagValuePair p,...){
	gcstack_push();

	va_list args;
	va_start(args,p);
	TagValuePair current=p,next=va_arg(args,TagValuePair);
	TagValuePair first={TAG_UNUSED,UNUSED};
	GCConsPtr terminal=UNUSED;

	while(1){
		if(next.tag==TAG_UNUSED){
			//引数が１つの時
			return current;
		}
		if(current.tag==TAG_PTR){
			first=current;
			break;
		}else if(current.tag==TAG_NIL){
			current=next;
			next=va_arg(args,TagValuePair);
		}else{
			error("The value is not of type LIST.");
		}
	}

	while(1){
		if(next.tag==TAG_UNUSED){
			break;
		}
		if(current.tag==TAG_PTR){
			terminal=current.value;
			while(DEREF(terminal).cdr.tag==TAG_PTR){
				terminal=DEREF(terminal).cdr.value;
			}
			DEREF(terminal).cdr=next;

			current=next;
			next=va_arg(args,TagValuePair);
		}else if(current.tag==TAG_NIL){
			current=next;
			next=va_arg(args,TagValuePair);
		}else{
			error("The value is not of type LIST.");
			break;
		}
	}

	va_end(args);

	gcstack_pop();
	return first;
}

TagValuePair make_continuation(TagValuePair s,TagValuePair e,TagValuePair c,TagValuePair d){
	TagValuePair cont=list(s,e,c,d,NULL);
	cont.tag=TAG_CONTINUATION;
	return cont;
}


TagValuePair eval(TagValuePair expr){
	static const void *itable[]={
		&&nil,&&nop,&&ref,&&tlval,&&push,&&pushclosure,&&tldef,
		&&pop,&&apply,&&bnil,&&pushflame,&&parambind,&&store,&&tlvalstore,
		&&dup,&&macdef,&&macval
	};

	CODE=compile(expr);

	while(1){
		goto *itable[caar(CODE).value];

		nil:
		{
			if(DUMP.tag==TAG_NIL){break;}

			TagValuePair lastcont=car(DUMP);
			STACK=cons(car(STACK),car(lastcont));
			ENVIRONMENT=cadr(lastcont);
			CODE=caddr(lastcont);
			DUMP=car(cdddr(lastcont));
			goto *itable[caar(CODE).value];
		}
		ref:
		{
			TagValuePair instarg=cdar(CODE);
			int flame=car(instarg).value,offset=cdr(instarg).value;
			TagValuePair current_flame=ENVIRONMENT,current_entry;

			for(;flame>0;flame--){
				current_flame=cdr(current_flame);
			}
			current_entry=car(current_flame);
			for(;offset>0;offset--){
				current_entry=cdr(current_entry);
			}

			STACK=cons(cdar(current_entry),STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		tlval:
		{
			TagValuePair instarg=cdar(CODE);
			TagValuePair d=sym_tlval(instarg.value);
			if(d.tag==TAG_UNUSED){
				error("The variable %s is unbound.",sym_ref(instarg.value));
			}else{
				STACK=cons(d,STACK);
				CODE=cdr(CODE);
			}
			goto *itable[caar(CODE).value];
		}
		push:
		{
			TagValuePair instarg=cdar(CODE);
			STACK=cons(instarg,STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		pushclosure:
		{
			TagValuePair instarg=cdar(CODE);
			TagValuePair closure=cons(instarg,ENVIRONMENT);
			closure.tag=TAG_CLOSURE;
			STACK=cons(closure,STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		pop:
		{
			STACK=cdr(STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		tldef:
		{
			TagValuePair instarg=cdar(CODE);
			sym_gettable(instarg.value)->toplevel_val=car(STACK);
			STACK=cdr(STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		parambind:
		{
			TagValuePair params=car(STACK);
			int paramlen=length(params);
			int argrest=cadr(STACK).value;
			int i,j;
			TagValuePair newenv=NIL,newpair;
			STACK=cddr(STACK);

			for(i=0;i<paramlen;i++,argrest--){
				if(car(params).tag==TAG_SYM && car(params).value==sym_get("&rest")){
					TagValuePair restlist=NIL;
					for(j=0;j<argrest;j++){
						restlist=nconc(restlist,list(car(STACK),NULL),NULL);
						STACK=cdr(STACK);
					}

					newpair=cons(cadr(params),restlist);
					newenv=cons(newpair,newenv);
					argrest=0;
					break;
				}else{
					if(argrest<=0){
						error("引数が不足しています。");
					}
					newpair=cons(car(params),car(STACK));
					STACK=cdr(STACK);
					params=cdr(params);
					newenv=cons(newpair,newenv);
				}
			}

			if(argrest>0){
				error("引数が多すぎます。");
			}

			ENVIRONMENT=cons(newenv,ENVIRONMENT);

			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		apply:
		{
			TagValuePair func=car(STACK);
			STACK=cdr(STACK);
			if(func.tag==TAG_SUBR){
				switch(func.value){
					case SUBR_CAR:
					{
						if(car(STACK).value!=1){
							error("wrong number of args to car");
						}
						STACK=cdr(STACK);
						TagValuePair target=car(STACK);
						if(target.tag!=TAG_PTR){
							error("pair required");
						}
						STACK=cons(car(target),cdr(STACK));
						CODE=cdr(CODE);
						break;
					}
					case SUBR_CDR:
					{
						if(car(STACK).value!=1){
							error("wrong number of args to cdr");
						}
						STACK=cdr(STACK);
						TagValuePair target=car(STACK);
						if(target.tag!=TAG_PTR){
							error("pair required");
						}
						STACK=cons(cdr(target),cdr(STACK));
						CODE=cdr(CODE);
						break;
					}
					case SUBR_CONS:
					{
						if(car(STACK).value!=2){
							error("wrong number of args to cons");
						}
						STACK=cdr(STACK);
						TagValuePair target1=car(STACK);
						TagValuePair target2=cadr(STACK);
						TagValuePair retval=cons(target1,target2);
						STACK=cons(retval,cddr(STACK));
						CODE=cdr(CODE);
						break;
					}
					case SUBR_ATOM:
					{
						if(car(STACK).value!=1){
							error("wrong number of args to atom");
						}
						STACK=cdr(STACK);
						TagValuePair target1=car(STACK);
						TagValuePair retval=(target1.tag==TAG_PTR?F:T);
						STACK=cons(retval,cdr(STACK));
						CODE=cdr(CODE);
						break;
					}
					case SUBR_EQ:
					{
						if(car(STACK).value!=2){
							error("wrong number of args to eq");
						}
						STACK=cdr(STACK);
						TagValuePair target1=car(STACK);
						TagValuePair target2=cadr(STACK);
						TagValuePair retval=(target1.tag==target2.tag&&target1.value==target2.value?T:F);
						STACK=cons(retval,cddr(STACK));
						CODE=cdr(CODE);
						break;
					}
					case SUBR_ADD:
					{
						int sum=0,len=car(STACK).value;
						STACK=cdr(STACK);
						for(;len>0;len--){
							if(car(STACK).tag!=TAG_INT){
								error("number required");
							}
							sum+=car(STACK).value;
							STACK=cdr(STACK);
						}
						STACK=cons(INTCONST(sum),STACK);
						CODE=cdr(CODE);
						break;
					}
					case SUBR_MUL:
					{
						int ans=1,len=car(STACK).value;
						STACK=cdr(STACK);
						for(;len>0;len--){
							if(car(STACK).tag!=TAG_INT){
								error("number required");
							}
							ans*=car(STACK).value;
							STACK=cdr(STACK);
						}
						STACK=cons(INTCONST(ans),STACK);
						CODE=cdr(CODE);
						break;
					}
					case SUBR_SUB:
					{
						int ans,len=car(STACK).value;
						if(len==0){
							error("wrong number of args to -");
						}
						STACK=cdr(STACK);
						ans=car(STACK).value;
						STACK=cdr(STACK);
						if(len==1){
							ans=-ans;
						}
						for(len--;len>0;len--){
							if(car(STACK).tag!=TAG_INT){
								error("number required");
							}
							ans-=car(STACK).value;
							STACK=cdr(STACK);
						}
						STACK=cons(INTCONST(ans),STACK);
						CODE=cdr(CODE);
						break;
					}
					case SUBR_DIV:
					{
						int ans,len=car(STACK).value;
						if(len!=2){
							error("wrong number of args to div");
						}
						STACK=cdr(STACK);
						ans=car(STACK).value/cadr(STACK).value;
						STACK=cddr(STACK);
						STACK=cons(INTCONST(ans),STACK);
						CODE=cdr(CODE);
						break;
					}
					case SUBR_MOD:
					{
						int ans,len=car(STACK).value;
						if(len!=2){
							error("wrong number of args to mod");
						}
						STACK=cdr(STACK);
						ans=car(STACK).value%cadr(STACK).value;
						STACK=cddr(STACK);
						STACK=cons(INTCONST(ans),STACK);
						CODE=cdr(CODE);
						break;
					}
					case SUBR_CALLCC:
					{
						if(car(STACK).value!=1){
							error("wrong number of args to call/cc");
						}
						TagValuePair f=cadr(STACK);
						if(f.tag!=TAG_CLOSURE && f.tag!=TAG_SUBR && f.tag!=TAG_CONTINUATION){
							error("callable object required to call/cc");
						}
						STACK=cons(make_continuation(cddr(STACK),ENVIRONMENT,cdr(CODE),DUMP),cddr(STACK));
						STACK=cons(INTCONST(1),STACK);
						STACK=cons(f,STACK);
						CODE=nconc(INST(INST_APPLY,NIL),cdr(CODE),NULL);
						break;
					}
					case SUBR_EVAL:
					{
						if(car(STACK).value!=1){
							error("wrong number of args to eval");
						}
						TagValuePair compiledcode=compile_expr(ENVIRONMENT,cadr(STACK));
						CODE=nconc(compiledcode,cdr(CODE),NULL);
						break;
					}
				}
			}else if(func.tag==TAG_CLOSURE){
				if(!(cdr(CODE).tag==TAG_NIL
					|| (cadr(CODE).tag==TAG_SYM && cadr(CODE).value==INST_NOP && cddr(CODE).tag==TAG_NIL))){
					//末尾呼び出しでない
					DUMP=cons(make_continuation(STACK,ENVIRONMENT,cdr(CODE),DUMP),DUMP);
				}
				ENVIRONMENT=cdr(func);
				CODE=car(func);
			}else if(func.tag==TAG_CONTINUATION){
				if(car(STACK).value!=1){
					error("wrong number of args to continuation");
				}
				TagValuePair passedvalue=cadr(STACK);
				STACK=cons(passedvalue,car(func));
				ENVIRONMENT=cadr(func);
				CODE=caddr(func);
				DUMP=car(cdddr(func));
			}else{
				error("Invalid application.");
			}
			goto *itable[caar(CODE).value];
		}
		nop:
		{
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		bnil:
		{
			TagValuePair instarg=cdar(CODE);
			if(car(STACK).tag==TAG_NIL || car(STACK).tag==TAG_F){
				CODE=instarg;
			}else{
				CODE=cdr(CODE);
			}
			STACK=cdr(STACK);
			goto *itable[caar(CODE).value];
		}
		pushflame:
		{
			ENVIRONMENT=cons(list(NIL,NULL),ENVIRONMENT);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		store:
		{
			TagValuePair instarg=cdar(CODE);
			int flame=car(instarg).value,offset=cdr(instarg).value;
			TagValuePair current_flame=ENVIRONMENT,current_entry;

			for(;flame>0;flame--){
				current_flame=cdr(current_flame);
			}
			current_entry=car(current_flame);
			for(;offset>0;offset--){
				current_entry=cdr(current_entry);
			}

			cdar(current_entry)=car(STACK);
			STACK=cdr(STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		tlvalstore:
		{
			TagValuePair instarg=cdar(CODE);
			SymbolTable* st=sym_gettable(instarg.value);
			if(st->toplevel_val.tag==TAG_UNUSED){
				error("The variable %s is not defined.",sym_ref(instarg.value));
			}else{
				st->toplevel_val=car(STACK);
				STACK=cdr(STACK);
				CODE=cdr(CODE);
			}

			goto *itable[caar(CODE).value];
		}
		dup:
		{
			STACK=cons(car(STACK),STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		macdef:
		{
			TagValuePair instarg=cdar(CODE);
			sym_gettable(instarg.value)->expander=car(STACK);
			STACK=cdr(STACK);
			CODE=cdr(CODE);
			goto *itable[caar(CODE).value];
		}
		macval:
		{
			TagValuePair instarg=cdar(CODE);
			TagValuePair d=sym_gettable(instarg.value)->expander;
			if(d.tag==TAG_UNUSED){
				error("The macro %s is not defined.",sym_ref(instarg.value));
			}else{
				STACK=cons(d,STACK);
				CODE=cdr(CODE);
			}
			goto *itable[caar(CODE).value];
		}
	}

	ENVIRONMENT=NIL;
	CODE=NIL;
	DUMP=NIL;
	TagValuePair retval=car(STACK);
	STACK=NIL;

	return retval;
}


TagValuePair compile(TagValuePair expr){
	return compile_expr(ENVIRONMENT,expr);
}

TagValuePair env_lookup_load(TagValuePair env,TagValuePair sym){
	int flame=0,offset;
	TagValuePair currentf=env;

	if(sym.tag!=TAG_SYM){
		error("The value is not symbol.");
	}

	while(currentf.tag!=TAG_NIL){
		if(currentf.tag==TAG_PTR){
			TagValuePair ptr= car(currentf);
			offset=0;
			while(ptr.tag!=TAG_NIL){
				if(caar(ptr).tag==TAG_SYM && caar(ptr).value==sym.value){
					//見つかった
					return INST(INST_REF,cons(INTCONST(flame),INTCONST(offset)));
				}
				offset++;
				ptr=cdr(ptr);
			}
		}
		flame++;
		currentf=cdr(currentf);
	}

	/*if(sym_tlval(sym.value).tag==TAG_UNUSED){
		error("The variable %s is undefined.",sym_ref(sym.value));
	}*/

	return INST(INST_TLVAL,sym);
}

TagValuePair env_lookup_store(TagValuePair env,TagValuePair sym){
	int flame=0,offset;
	TagValuePair currentf=env;

	if(sym.tag!=TAG_SYM){
		error("The value is not symbol.");
	}

	while(currentf.tag!=TAG_NIL){
		if(currentf.tag==TAG_PTR){
			TagValuePair ptr= car(currentf);
			offset=0;
			while(ptr.tag!=TAG_NIL){
				if(caar(ptr).tag==TAG_SYM && caar(ptr).value==sym.value){
					//見つかった
					return INST(INST_STORE,cons(INTCONST(flame),INTCONST(offset)));
				}
				offset++;
				ptr=cdr(ptr);
			}
		}
		flame++;
		currentf=cdr(currentf);
	}

	/*if(sym_tlval(sym.value).tag==TAG_UNUSED){
		error("The variable %s is undefined.",sym_ref(sym.value));
	}*/

	return INST(INST_TLVALSTORE,sym);
}

TagValuePair compile_expr(TagValuePair env,TagValuePair expr){
	TagValuePair ret={TAG_PTR,UNUSED};

	switch(expr.tag){
		case TAG_SYM:
			ret=env_lookup_load(env,expr);
			break;
		case TAG_PTR:
			ret=compile_list(env,expr);
			break;
		case TAG_INT:
			ret=INST(INST_PUSH,expr);
			break;
		case TAG_NIL:
			ret=INST(INST_PUSH,NIL);
			break;
		case TAG_T:
			ret=INST(INST_PUSH,T);
			break;
		case TAG_F:
			ret=INST(INST_PUSH,F);
			break;
		default:
			ret.tag=TAG_UNUSED;
			ret.value=UNUSED;
			break;
	}

	return ret;
}

TagValuePair compile_list(TagValuePair env,TagValuePair expr){
	TagValuePair ret={TAG_UNUSED};

	TagValuePair first=car(expr);
	int arglen=length(cdr(expr));

	if(first.tag==TAG_SYM){
		if(first.value==SYM_if.value){
			ret=compile_if(env,cdr(expr));
			return ret;
		}else if(first.value==SYM_quote.value){
			if(arglen!=1){
				error("wrong number of args to quote");
			}
			ret=INST(INST_PUSH,cadr(expr));
			return ret;
		}else if(first.value==SYM_quasiquote.value){
			if(arglen!=1){
				error("wrong number of args to quasiquote");
			}
			ret=INST(INST_PUSH,cadr(expr));
			return ret;
		}else if(first.value==SYM_unquote.value){
			error("unquote appeared outside quasiquote");
		}else if(first.value==SYM_unquote_splicing.value){
			error("unquote-splicing appeared outside quasiquote");
		}else if(first.value==SYM_lambda.value){
			if(arglen<=0){
				error("wrong number of args to lambda");
			}
			ret=compile_lambda(env,cdr(expr));
			return ret;
		}else if(first.value==SYM_define.value){
			if(arglen!=2){
				error("wrong number of args to define");
			}

			if(env.tag!=TAG_NIL){
				error("define can only appear in the toplevel");
			}
			return nconc(compile_expr(env,caddr(expr)),INST(INST_TLDEF,cadr(expr)),INST(INST_PUSH,cadr(expr)),NULL);
		}else if(first.value==SYM_begin.value){
			TagValuePair remain=cdr(expr);
			ret=NIL;
			while(remain.tag!=TAG_NIL){
				if(cdr(remain).tag==TAG_NIL){
					//最後の引数(ポップしない)
					ret=nconc(ret,compile_expr(env,car(remain)),NULL);
				}else{
					ret=nconc(ret,compile_expr(env,car(remain)),INST(INST_POP,NIL),NULL);
				}

				remain=cdr(remain);
			}
			return ret;
		}else if(first.value==SYM_set.value){
			if(arglen!=2){
				error("wrong number of args to set!");
			}
			return nconc(compile_expr(env,caddr(expr)),INST(INST_DUP,NIL),env_lookup_store(env,cadr(expr)),NULL);
		}else if(first.value==SYM_defmacro.value){
			if(arglen<2){
				error("wrong number of args to defmacro");
			}

			if(env.tag!=TAG_NIL){
				error("defmacro can only appear in the toplevel");
			}
			return nconc(compile_lambda(env,cddr(expr)),INST(INST_MACDEF,cadr(expr)),INST(INST_PUSH,cadr(expr)),NULL);
		}else if(first.value==SYM_defun.value){
			if(arglen<2){
				error("wrong number of args to defun");
			}

			if(env.tag!=TAG_NIL){
				error("defun can only appear in the toplevel");
			}
			return nconc(compile_lambda(env,cddr(expr)),INST(INST_TLDEF,cadr(expr)),INST(INST_PUSH,cadr(expr)),NULL);
		}else if(sym_gettable(first.value)->expander.tag!=TAG_UNUSED){
			TagValuePair arglist=compile_arguments_noeval(cdr(expr));
			return nconc(arglist,INST(INST_PUSH,sym_gettable(first.value)->expander),INST(INST_APPLY,NIL),NULL);
		}
	}

	TagValuePair arglist=compile_arguments(env,cdr(expr));
	ret=nconc(arglist,compile_expr(env,first),INST(INST_APPLY,NIL),NULL);
	return ret;
}

TagValuePair compile_if(TagValuePair env,TagValuePair expr){
	int len=length(expr);
	TagValuePair ret={TAG_UNUSED};
	TagValuePair cond;
	TagValuePair truepart;
	TagValuePair falsepart;
	TagValuePair merge;

	if(len==2){
		cond=compile_expr(env,car(expr));
		merge=INST(INST_NOP,NIL);
		truepart=compile_expr(env,cadr(expr));

		ret=nconc(cond,INST(INST_BNIL,nconc(INST(INST_PUSH,NIL),merge,NULL)),truepart,merge,NULL);
	}else if(len==3){
		cond=compile_expr(env,car(expr));
		merge=INST(INST_NOP,NIL);
		truepart=compile_expr(env,cadr(expr));
		falsepart=nconc(compile_expr(env,caddr(expr)),merge,NULL);

		ret=nconc(cond,INST(INST_BNIL,falsepart),
				truepart,merge,NULL);
	}else{
		error("wrong number of args to if");
	}


	return ret;
}

TagValuePair compile_lambda(TagValuePair env,TagValuePair expr){
	TagValuePair newenv=NIL,params=car(expr);
	while(params.tag!=TAG_NIL){
		if(car(params).tag==TAG_SYM && car(params).value==sym_get("&rest")){
			params=cdr(params);
			if(cdr(params).tag!=TAG_NIL){
				error("&rest parameter must be last parameter.");
			}
		}
		newenv=cons(cons(car(params),NIL),newenv);
		params=cdr(params);
	}
	TagValuePair data=nconc(INST(INST_PUSH,car(expr)),INST(INST_PARAMBIND,NIL),
					compile_list(cons(newenv,env),nconc(list(SYM_begin,NULL),cdr(expr),NULL)),NULL);
	return INST(INST_PUSHCLOSURE,data);
}

TagValuePair compile_arguments(TagValuePair env,TagValuePair args){
	TagValuePair ret={TAG_NIL,UNUSED};
	TagValuePair rest=args;
	int count=0;

	while(rest.tag!=TAG_NIL){
		ret=nconc(compile_expr(env,car(rest)),ret,NULL);
		count++;
		rest=cdr(rest);
	}

	ret=nconc(ret,INST(INST_PUSH,INTCONST(count)),NULL);
	return ret;

}

TagValuePair compile_arguments_noeval(TagValuePair args){
	TagValuePair ret={TAG_NIL,UNUSED};
	TagValuePair rest=args;
	int count=0;

	while(rest.tag!=TAG_NIL){
		ret=nconc(INST(INST_PUSH,car(rest)),ret,NULL);
		count++;
		rest=cdr(rest);
	}

	ret=nconc(ret,INST(INST_PUSH,INTCONST(count)),NULL);
	return ret;

}

TagValuePair expand_macro(TagValuePair expr){
	//printf("\n---expandstart\n");
	//print(expr);printf("\n");
	TagValuePair result=NIL;
	if(expr.tag!=TAG_PTR){
		//非リスト
		//printf("---expandend\n");
		return expr;
	}
	while(car(expr).tag==TAG_SYM && sym_gettable(car(expr).value)->expander.tag!=TAG_UNUSED){
		expr=eval(expr);
	}
	if(expr.tag!=TAG_PTR){
		//非リスト
		//printf("---expandend\n");
		return expr;
	}

	if(expr.tag==TAG_SYM && expr.value==SYM_quote.value){
		return expr;
	}else if(expr.tag==TAG_SYM && (expr.value==SYM_define.value || expr.value==SYM_defmacro.value || expr.value==SYM_defun.value)){
		//名前と引数リストは展開対象から外す
		result=list(car(expr),cadr(expr),caddr(expr),NULL);
		expr=cdddr(expr);
	}else if(expr.tag==TAG_SYM && expr.value==SYM_lambda.value){
		//引数リストは展開対象から外す
		result=list(car(expr),cadr(expr),NULL);
		expr=cddr(expr);
	}else{
		result=list(car(expr),NULL);
		expr=cdr(expr);
	}

	//print(expr);
	//printf("\n");

	while(expr.tag==TAG_PTR){
		result=nconc(result,list(expand_macro(car(expr)),NULL),NULL);
		expr=cdr(expr);
	}
	result=nconc(result,expr,NULL);
	//print(result);
	//printf("\n---expandend\n");
	return result;
}
