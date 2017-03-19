#pragma once

#include <stdio.h>
#include <stdlib.h>

#define SYMTABLE_LEN 256
#define IDENT_MAX_STR_LEN 256

#define DEREF(ptr) (fromspace[ptr])

#define UNUSED -1

#define car(e) DEREF(e.value).car
#define cdr(e) DEREF(e.value).cdr
#define caar(e) car(car(e))
#define cadr(e) car(cdr(e))
#define cdar(e) cdr(car(e))
#define cddr(e) cdr(cdr(e))
#define caaar(e) car(car(car(e)))
#define caadr(e) car(car(cdr(e)))
#define cadar(e) car(cdr(car(e)))
#define caddr(e) car(cdr(cdr(e)))
#define cdaar(e) cdr(car(car(e)))
#define cdadr(e) cdr(car(cdr(e)))
#define cddar(e) cdr(cdr(car(e)))
#define cdddr(e) cdr(cdr(cdr(e)))

#define error(...) \
  do { \
    fprintf(stderr,"ERROR:"); \
    fprintf(stderr,__VA_ARGS__); \
	getc(stdin); \
	fprintf(stderr,"\n"); \
	exit(-1);} \
  while (0)

#define SETSUBR(name,val) \
  do { \
	sym_gettable(sym_get(name))->toplevel_val.tag=TAG_SUBR; \
    sym_gettable(sym_get(name))->toplevel_val.value=val;} \
  while (0)

typedef int GCConsPtr; //UNUSEDで必ず初期化(GCのために)

typedef enum{TOK_INTVAL,TOK_SYMBOL,TOK_ASCII,TOK_ENDOFFILE} TokenTag;
typedef enum{TAG_UNUSED,TAG_NIL/*ここまで固定*/,TAG_PTR,TAG_COMPILEDCODE,TAG_INT,TAG_INST,TAG_SYM,TAG_BROKENHEART,
			TAG_CONTINUATION,TAG_T,TAG_F,TAG_CHAR,TAG_CLOSURE,TAG_SUBR} Tag;
typedef enum{SUBR_CAR,SUBR_CDR,SUBR_CONS,SUBR_ATOM,SUBR_EQ,SUBR_ADD,SUBR_MUL,SUBR_SUB,SUBR_DIV,SUBR_MOD,
				SUBR_CALLCC,SUBR_EVAL} Subr;

typedef struct _gcentry{
    struct _gcentry* next;
	GCConsPtr* ptr;
	int use_tag;
	unsigned char* tag;
} GCEntry;

typedef struct _gcstack{
    struct _gcstack* next;
    GCEntry entry;
} GCStack;

typedef struct _tagval_pair{
	unsigned char tag; //TAG_UNUSEDで必ず初期化(GCのために)
	int value;
} TagValuePair;

typedef struct _symtable{
	struct _symtable* next;
	char* name;
	TagValuePair toplevel_val;
	TagValuePair expander;
} SymbolTable;

typedef union _symptr{
	struct{
		unsigned int hash:8;
		unsigned int pos:24;
	} part;
	int value;
} SymbolPtr;


typedef struct _token{
	TokenTag tag;
	union{
		int intval;
		int symbol;
		char ascii;
	} value;
} Token;



typedef struct _cons{
	TagValuePair car;
	TagValuePair cdr;
} Cons;


//main.c


//etc.c
extern TagValuePair NIL;
extern TagValuePair T;
extern TagValuePair F;
extern TagValuePair TERMINAL;
TagValuePair INTCONST(int v);
TagValuePair SYMCONST(char* str);
TagValuePair SYMLIST(char* str);
TagValuePair INST(int name,TagValuePair arg);

//symbol.c
extern SymbolTable symtable[SYMTABLE_LEN];
int sym_get(char* str);
char* sym_ref(int ptr);
TagValuePair sym_tlval(int ptr);
SymbolTable* sym_gettable(int ptr);
int sym_get_anonymousvar();
int sym_get_labelvar();

//lexer.c
Token token_get(FILE* fp);
void token_unget(Token t);

//parser.c
TagValuePair read(FILE* fp);

//print.c
void print(TagValuePair pair);

//gc.c
extern Cons* fromspace;
extern Cons* tospace;
void gc_init();
void gcstack_push();
void gcstack_pop();
void gc_watchvar_ptr(GCConsPtr* var,...);
void gc_watchvar_pair(TagValuePair* var,...);
GCConsPtr gc_getcons();
void gc_copy();

//eval.c
typedef enum{INST_NIL,INST_NOP/*NILの場合これになり、終端を検出できる*/,INST_REF,INST_TLVAL,INST_PUSH,INST_PUSHCLOSURE,INST_TLDEF,INST_POP,
			INST_APPLY,INST_BNIL,INST_PUSHFLAME,INST_PARAMBIND,INST_STORE,INST_TLVALSTORE,INST_DUP,INST_MACDEF,INST_MACVAL} Instruction;


void evaluator_init();
TagValuePair cons(TagValuePair car,TagValuePair cdr);
TagValuePair nconc(TagValuePair p,...);
TagValuePair list(TagValuePair p,...);
TagValuePair eval(TagValuePair expr);
TagValuePair compile(TagValuePair expr);
TagValuePair compile_expr(TagValuePair env,TagValuePair expr);
TagValuePair compile_lambda(TagValuePair env,TagValuePair expr);
TagValuePair expand_macro(TagValuePair expr);
