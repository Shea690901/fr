#include "efuns.h"
#include "stralloc.h"
#include "include/origin.h"

#ifdef LPC_TO_C
extern int too_deep_error, max_eval_error;
extern int call_origin;
extern int error_recovery_context_exists;

#define RTN (*ret = *sp--)

#ifdef TRACE
int c_do_trace(msg, fname, post)
char *msg, *fname, *post;
{
  if (TRACEP(TRACE_LPC_EXEC)) do_trace(msg,fname,post);
}
#endif

int c_return(struct svalue *ret, struct svalue *x) {
  assign_svalue_no_free(ret,x);
  pop_n_elems(csp->num_local_variables);
#ifdef DEBUG
  if (sp != fp-1) debug_fatal("Bad stack in c_return\n");
#endif
  pop_control_stack();
}

int c_catch_start() {
  if (max_eval_error)
    error("Can't catch eval cost too big error.\n");
  if (too_deep_error)
    error("Can't catch too deep recursion error.\n");
  
  push_control_stack(0);
  csp->num_local_variables = (csp-1)->num_local_variables; /* marion */
  push_pop_error_context(1);
  
  error_recovery_context_exists = 2;
}

int c_catch_error() {
  push_pop_error_context(-1);
  pop_control_stack();
}

int c_catch_end() {
  assign_svalue(&catch_value, &const0);
  pop_control_stack();
  push_pop_error_context(0);
}

#ifdef DEBUG
int c_debug(char *file, int line, int on_stack) {
  if (sp < fp+csp->num_local_variables+on_stack-1) error("Bad stack in LPC code at %s:%i.",file,line);
}
#endif

/* This is used by sscanf and parse_command (lvalue_list) */
void
C_ASSIGN_FROM_STACK(svalue *lvalue) {
  if (sp->type == T_INVALID) {
    pop_stack();
    return;
  } else {
    if (lvalue->type == T_LVALUE_BYTE) {
      if (sp->type != T_NUMBER) {
	*lvalue->u.lvalue_byte = 0;
	pop_stack();
      } else {
	*lvalue->u.lvalue_byte = (sp->u.number & 0xff);
	sp--;
      }
    } else {
      /* Be clever here */
      free_svalue(lvalue);
      *lvalue = *sp--;
    }
  }
}

svalue *
check_dested(svalue *x) {
  return (x->type == T_OBJECT && (x->u.ob->flags & O_DESTRUCTED) ? 0 : x);
}

svalue *
c_index(svalue *vec, svalue *i, svalue *tmp)
{
  int idx;
  
  if (vec->type == T_MAPPING) {
    struct svalue *v;

    v = find_in_mapping(vec->u.map, i);
    return check_dested(v);
  }

  if (i->type != T_NUMBER || (idx=i->u.number)<0) {
    error("Indexing a buffer with an illegal type.\n");
  }

  if (vec->type == T_BUFFER) {
    tmp->type = T_NUMBER;
    tmp->subtype = 0;
    if ((idx > vec->u.buf->size) || (idx < 0)) {
      error("Buffer index out of bounds.\n");
    }
    tmp->u.number = vec->u.buf->item[idx];
    return tmp;
  } else if (vec->type == T_STRING) {
    struct svalue w;
    int idx;

    idx = i->u.number;
    if (i->type != T_NUMBER) {
      error("Indexing a string with an illegal type.\n");
    }
    tmp->type = T_NUMBER;
    tmp->subtype = 0;
    if ((idx > SVALUE_STRLEN(vec)) || (idx < 0)) {
      error("String index out of bounds.\n");
    }
    tmp->u.number = (unsigned char)vec->u.string[idx];
    return tmp;
  } else if (vec->type == T_FUNCTION) {
    if (idx>1) {
      error("Function variables may only be indexed with 0 or 1.\n");
    }
    return (idx ? &vec->u.fp->fun : check_dested(&vec->u.fp->obj));
  } else if (vec->type == T_POINTER) {
    if (idx >= vec->u.vec->size) error ("Index out of bounds\n");
    return check_dested(&vec->u.vec->item[idx]);
  } else
    error("Indexing on illegal type.\n");
}

void
c_range(svalue *ret, svalue *base, svalue *start, svalue *end) {
  if (start->type != T_NUMBER)
    error("Start of range [ .. ] interval must be a number.\n");
  if (end->type != T_NUMBER)
    error("End of range [ .. ] interval must be a number.\n");
  if (base->type == T_STRING) {
    int len, from, to;
    char *res;

    len = strlen(base->u.string);
    from = start->u.number;
    if (from < 0)
      from += len;
    if (from > len) {
      C_STRING(ret, "", STRING_CONSTANT);
      return;
    }
    to = end->u.number;
    if (to < 0) 
      to += len;
    if (to < from) {
      C_STRING(ret, "", STRING_CONSTANT);
      return;
    }
    if (to >= len)
      to = len-1;
    if (to == len-1) {
      res = string_copy(base->u.string + from);
      C_MALLOCED_STRING(ret,res);
      return;
    }
    res = DXALLOC(to - from + 2, 37, "c_range");
    strncpy(res, base->u.string + from, to - from + 1);
    res[to - from + 1] = '\0';
    C_MALLOCED_STRING(ret, res);
  } else if (base->type == T_BUFFER) {
    int len, from, to;
    struct buffer *rbuf;

    len = base->u.buf->size;
    from = start->u.number;
    if (from < 0)
      from += len;
    if (from >= len) {
      C_BUFFER(ret, null_buffer());
      return;
    }
    to = end->u.number;
    if (to < 0)
      to += len;
    if (to < from) {
      C_BUFFER(ret, null_buffer());
      return;
    }
    if (to >= len)
      to = len-1;
    rbuf = allocate_buffer(to - from + 1);
    rbuf->ref--;
    memcpy(rbuf->item, base->u.buf->item + from, to - from + 1);
    C_BUFFER(ret, rbuf);
  } else if (base->type == T_POINTER) {
    struct vector *v;

    v = slice_array(base->u.vec, start->u.number, end->u.number);
    if (v) {
      C_VECTOR(ret, v);
      v->ref--;
    } else
      *ret = const0;
  } else
    error("Bad argument to [ .. ] range operator.\n");
}

void
c_ne(svalue *ret, svalue *s0, svalue *s1) {
  int i;

  ret->type = T_NUMBER;
  switch(s1->type){
  case T_NUMBER:
    if(s0->type == T_NUMBER){
      ret->u.number = (s0->u.number != s1->u.number);
    }
    else if(s0->type == T_REAL){
      ret->u.number = (s0->u.real != s1->u.number);
    }
    else ret->u.number = 1;
    break;
  case T_REAL:
    if(s0->type == T_NUMBER){
      ret->u.number = (s0->u.number != s1->u.real);
    }
    else if(s0->type == T_REAL){
      ret->u.number = (s0->u.real != s1->u.real);
    }
    else ret->u.number = 1;
    break;
  case T_POINTER:
    if(s0->type == T_POINTER){
      ret->u.number = (s0->u.vec != s1->u.vec);
    }
    else ret->u.number = 1;
    break;
  case T_STRING:
    if(s0->type == T_STRING){
      ret->u.number = strcmp(s0->u.string, s1->u.string);
    }
    else ret->u.number = 1;
    break;
  case T_OBJECT:
    if(s0->type == T_OBJECT){
      ret->u.number = (s0->u.ob != s1->u.ob);
    }
    else ret->u.number = 1;
    break;
  case T_MAPPING:
    if(s0->type == T_MAPPING){
      ret->u.number = (s0->u.map != s1->u.map);
    }
    else ret->u.number = 1;
    break;
  default:
    ret->u.number = 1;
    break;
  }
}

/* enhanced call_other written 930314 by Luke Mewburn <zak@rmit.edu.au> */
void
c_call_other(svalue *ret, svalue *s0, svalue *s1, int num_arg)
{
  struct object *ob;
    char *funcname;
    int i;

    if (current_object->flags & O_DESTRUCTED)
    {				/* No external calls allowed */
	pop_n_elems(num_arg);
	C_UNDEFINED(ret);
	return;
    }
    if (s1->type == T_STRING)
	funcname = s1->u.string;
    else		/* must be T_POINTER then */
    {
        void check_for_destr(struct vector*);
	
	check_for_destr(s1->u.vec);
	if ( (s1->u.vec->size < 1)
	  || (s1->u.vec->item[0].type != T_STRING) )
	    error("call_other: 1st elem of array for arg 2 must be a string\n");
	funcname = s1->u.vec->item[0].u.string;	/* complicated huh? */
	for (i = 1; i < s1->u.vec->size; i++)
	    push_svalue(&s1->u.vec->item[i]);
	num_arg += i-1;	/* hopefully that will work */
    }
    if (strchr(funcname,':') != NULL)
	error("Illegal function name in call_other: %s\n", s1->u.string);
    if (s0->type == T_OBJECT)
	ob = s0->u.ob;
    else if (s0->type == T_POINTER)
    {
        struct vector *v;
        extern struct vector *call_all_other PROT((struct vector *, char *,int));


	v = call_all_other(s0->u.vec, funcname, num_arg);
        C_VECTOR(ret,v);
        v->ref--;
	return;
    }
    else
    {
	ob = find_object(s0->u.string);
	if (!ob || !object_visible(ob))
	    error("call_other() couldn't find object\n");
    }
	    /* Send the remaining arguments to the function. */
#ifdef TRACE
    if (TRACEP(TRACE_CALL_OTHER))
    {
	do_trace("Call other ", funcname, "\n");
    }
#endif
  call_origin = ORIGIN_CALL_OTHER;
  if (apply_low(funcname, ob, num_arg) == 0)
    {				/* Function not found */
      C_UNDEFINED(ret);
      return;
    }

  *ret = *sp--;
  return;
}

void c_simul_efun(svalue *ret, svalue *s0, int num_arg) {
  char *funcname;
  int i;
  extern struct object *simul_efun_ob;
  extern char *simul_efun_file_name;

  if (current_object->flags & O_DESTRUCTED)
    {  /* No external calls allowed */
      pop_n_elems(num_arg);
      push_undefined();
      return;
    }
  funcname = s0->u.string;
  
  if (!simul_efun_ob || (simul_efun_ob->flags & O_DESTRUCTED)) {
    simul_efun_ob = find_object(simul_efun_file_name);
    /* if it didn't load we're in trouble. */
    if (!simul_efun_ob || (simul_efun_ob->flags & O_DESTRUCTED)) 
      error("No simul_efun object for simul_efun\n");
  }
  
#ifdef TRACE
  if (TRACEP(TRACE_CALL_OTHER)) {
    do_trace("simul_efun ", funcname, "\n");
  }
#endif
  call_origin = ORIGIN_SIMUL_EFUN;
  if (apply_low(funcname, simul_efun_ob, num_arg-1) == 0)
    { /* Function not found */
      error("%s is no longer a simul_efun.\n", funcname);
    }
  
  *ret = *sp--;
}

void
c_sscanf(svalue *ret, svalue *s0, svalue *s1, int num_arg)
{
  int i;
  /* prepare space on stack for rvalues */
  sp += num_arg;

  for (i=0; i<num_arg; i++)
    (sp-i)->type = T_INVALID;

  i = inter_sscanf(sp,s0,s1,num_arg);
  C_NUMBER(ret,i);
}

void
c_parse_command(svalue *ret, svalue *s0, svalue *s1, svalue *s2, int num_arg)
{
  void check_for_destr(struct vector *);
  int i;
  struct svalue *fp = sp;

  if (s0->type != T_STRING)
    bad_arg(1, F_PARSE_COMMAND);
  if (s1->type != T_OBJECT && s1->type != T_POINTER)
    bad_arg(2, F_PARSE_COMMAND);
  if (s2->type != T_STRING)
    bad_arg(3, F_PARSE_COMMAND);

  if (s1->type == T_POINTER)
    check_for_destr(s1->u.vec);
  
  /* prepare space on stack for rvalues */
  sp += num_arg;

  for (i=0; i<num_arg; i++)
    (sp-i)->type = T_INVALID;

  i = parse(s0->u.string, s1, s2->u.string, fp, num_arg);

  C_NUMBER(ret, i);    
}

C_PUSH_LVALUE(struct svalue *s0) {
  sp++;
  sp->type = T_LVALUE;
  sp->u.lvalue = s0;
}

C_UNDEFINED(struct svalue *ret) {
  *ret=const0u;
}

C_AGGREGATE(struct svalue *ret,int num) {
  struct vector *v;
  int i;

  v = allocate_array(num);
  /*
   * transfer svalues in reverse, popping stack as we go
   */
  for (i=num; i--; sp--) 
    v->item[i] = *sp;
  ret->type = T_POINTER;
  ret->u.vec = v;
}

C_TIME_EXPRESSION() {
  long sec, usec;

/* uses the stack so it's as slow as real time_expr */
  get_usec_clock(&sec, &usec);
  push_number(sec);
  push_number(usec);
}

C_END_TIME_EXPRESSION(svalue *ret, svalue *s0) {
  long sec, usec;
  get_usec_clock(&sec, &usec);
  usec = (sec - (sp-1)->u.number) * 1000000 + (usec - (sp->u.number));
  sp -= 2;
  C_NUMBER(ret, usec);
}

C_ASSOC(struct svalue *ret,int num) {
  struct mapping *m;
  m = load_mapping_from_aggregate(sp-num+1,num);
  pop_n_elems(num);
  ret->type = T_MAPPING;
  ret->u.map = m;
}

C_CALL(struct svalue *ret, unsigned short numargs, unsigned short func_index) {
  struct function *funp;
  
  func_index += function_index_offset;
  /*
   * Find the function in the function table. As the function may have
   * been redefined by inheritance, we must look in the last table,
   * which is pointed to by current_object.
   */
#ifdef DEBUG
  if (func_index >= current_object->prog->p.i.num_functions)
    fatal("Illegal function index\n");
#endif
  
  /* NOT current_prog, which can be an inherited object. */
  funp = &current_object->prog->p.i.functions[func_index];
  
  if (funp->flags & NAME_UNDEFINED)
    error("Undefined function: %s\n", funp->name);
  /* Save all important global stack machine registers */
  push_control_stack(funp);	/* return pc is adjusted later */
  
  caller_type = ORIGIN_LOCAL;
  /* This assigment must be done after push_control_stack() */
  current_prog = current_object->prog;
  /*
   * If it is an inherited function, search for the real
   * definition.
   */
  csp->num_local_variables = numargs;
  funp = setup_new_frame(funp);
  if (current_prog->p.i.program_size) {
    eval_instruction(current_prog->p.i.program+funp->offset);
    RTN;
  } else {
    (*
     ( void (*)() )(funp->offset)
     )(ret);
  }
}

C_PROG_STRING(svalue *ret, int string_number) {
  ret->type = T_STRING;
  ret->subtype = STRING_CONSTANT;
  ret->u.string = current_prog->p.i.strings[string_number];
}

INLINE int C_EVAL_NUMBER(svalue *s0) {
  if (s0->type != T_NUMBER)
    bad_arg(1, F_SWITCH);
  return s0->u.number;
}

INLINE int C_IS_FALSE(svalue *s0) {
  return (s0->type == T_NUMBER) && (s0->u.number == 0);
}

INLINE int C_IS_TRUE(svalue *s0) {
  return (s0->type != T_NUMBER) || (s0->u.number);
}

INLINE int C_SV_FALSE(svalue *s0) {
  return (s0->type != T_NUMBER ? (free_svalue(s0),0) : s0->u.number);
}

INLINE int C_SV_TRUE(svalue *s0) {
  return (s0->type != T_NUMBER ? (free_svalue(s0),1) : s0->u.number);
}

INLINE
void C_OBJECT(ret,ob)
struct svalue *ret;
struct object *ob;
{
  if (ob) {
    ret->type = T_OBJECT;
    ret->u.ob = ob;
    add_ref(ob, "C_OBJECT");
  } else {
    ret->type = T_NUMBER;
    ret->subtype = T_NULLVALUE;
    ret->u.number = 0;
  }
}

C_FUNCTION(svalue *ret, struct funp *fp) {
  fp->ref++;
  ret->type = T_FUNCTION;
  ret->u.fp = fp;
}

C_NUMBER(svalue *ret, int n) {
  ret->type = T_NUMBER;
  ret->subtype = 0;
  ret->u.number = n;
}
 
C_REAL(svalue *ret, double r) {
  ret->type = T_REAL;
  ret->u.real = r;  
}

C_BUFFER(svalue *ret, struct buffer *b) {
  ret->type = T_BUFFER;
  ret->u.buf = b;
  b->ref++;
}

C_MAPPING(svalue *ret, struct mapping *m) {
  ret->type = T_MAPPING;
  ret->u.map = m;
  m->ref++;
}

C_STRING(svalue *ret, char *p, int type) {
  ret->type = T_STRING;
  ret->subtype = type;
  switch (type) {
  case STRING_MALLOC:
    ret->u.string = string_copy(p);
    break;
  case STRING_SHARED:
    ret->u.string = make_shared_string(p);
    break;
  case STRING_CONSTANT:
    ret->u.string = p;
    break;
  }
}

C_MALLOCED_STRING(svalue *ret, char *p) {
  ret->type = T_STRING;
  ret->subtype = STRING_MALLOC;
  ret->u.string = p;
}

C_CONSTANT_STRING(svalue *ret, char *p) {
  ret->type = T_STRING;
  ret->subtype = STRING_CONSTANT;
  ret->u.string = p;
}

C_VECTOR(svalue *ret, struct vector *v) {
  ret->type = T_POINTER;
  ret->u.vec = v;
  v->ref++;
}

void
c_function_constructor(svalue *ret, svalue *s0, svalue *s1)
{
  struct funp *fp;
  
  fp = make_funp(s0, s1);
  C_FUNCTION(ret,fp);
  fp->ref--;
}

/* Yes, this is an odd prototype, due to the fact (*) returns two values */
void
c_function_split(svalue *s0, svalue *ret1, svalue *ret2) {
  struct svalue *obj, *fun;

  if (s0->type != T_FUNCTION) {
    bad_arg(1, F_FUNCTION_SPLIT);
  }
  obj = &sp->u.fp->obj;
  fun = &sp->u.fp->fun;

  if ((obj->type == T_OBJECT) && (obj->u.ob->flags & O_DESTRUCTED)) {
    assign_svalue(obj, &const0n);
  }
  assign_svalue_no_free(ret1, obj);
  assign_svalue_no_free(ret2, fun);
}

void
c_this_function_constructor(svalue *ret, svalue *s0)
{
  struct funp *fp;

  if (current_object->flags & O_DESTRUCTED)
    C_NUMBER(ret,0);
  else
    C_OBJECT(ret,current_object);
  fp = make_funp(ret , s0);
  C_FUNCTION(ret,fp);
  fp->ref--;
}

void c_or(svalue *ret, svalue *s0, svalue *s1) {
  if (s0->type != T_NUMBER)
    bad_arg(1, F_OR);
  if (s1->type != T_NUMBER)
    bad_arg(2, F_OR);
  C_NUMBER(ret, s0->u.number | s1->u.number);
}

void c_xor(svalue *ret, svalue *s0, svalue *s1) {
  if (s0->type != T_NUMBER)
    bad_arg(1, F_XOR);
  if (s1->type != T_NUMBER)
    bad_arg(2, F_XOR);
  C_NUMBER(ret, s0->u.number ^ s1->u.number);
}

void c_and(svalue *ret, svalue *s0, svalue *s1) {
  if (s1->type == T_POINTER && s0->type == T_POINTER)
    {
      extern struct vector *intersect_array
	PROT((struct vector *, struct vector *));
      
      s0->u.vec->ref++; s1->u.vec++;
      C_VECTOR(ret, intersect_array(s0->u.vec, s1->u.vec));
      return;
    }
  if (s0->type != T_NUMBER)
    bad_arg(1, F_AND);
  if (s1->type != T_NUMBER)
    bad_arg(2, F_AND);
  C_NUMBER(ret, s0->u.number & s1->u.number);
}

void c_eq(svalue *ret, svalue *s0, svalue *s1) {
  ret->type = T_NUMBER;
  switch(s1->type){
  case T_NUMBER:
    if(s0->type == T_NUMBER){
      ret->u.number = (s0->u.number == s1->u.number);
    }
    else if(s0->type == T_REAL){
      ret->u.number = (s0->u.real == s1->u.number);
    }
    else ret->u.number =0;
    break;
  case T_REAL:
    if(s0->type == T_NUMBER){
      ret->u.number = (s0->u.number == s1->u.real);
    }
    else if(s0->type == T_REAL){
      ret->u.number = (s0->u.real == s1->u.real);
    }
    else ret->u.number = 0;
    break;
  case T_POINTER:
    if(s0->type == T_POINTER){
      ret->u.number = (s0->u.vec == s1->u.vec);
    }
    else ret->u.number = 0;
    break;
  case T_STRING:
    if(s0->type == T_STRING){
      ret->u.number = (strcmp(s0->u.string, s1->u.string) == 0);
    }
    else ret->u.number = 0;
    break;
  case T_OBJECT:
    if(s0->type == T_OBJECT){
      ret->u.number = (s0->u.ob == s1->u.ob);
    }
    else ret->u.number = 0;
    break;
  case T_MAPPING:
    if(s0->type == T_MAPPING){
      ret->u.number = (s0->u.map == s1->u.map);
    }
    else ret->u.number = 0;
    break;
  default:
    ret->u.number = 0;
    break;
  }
}

void c_gt(svalue *ret, svalue *s0, svalue *s1) {
  ret->type = T_NUMBER;
  if(s0->type == T_NUMBER){
    if(s1->type == T_NUMBER){
      ret->u.number = (s0->u.number > s1->u.number);
    }
    else if(s1->type == T_REAL){
      ret->u.number = (s0->u.number > s1->u.real);
    }
    else {
      bad_arg(2, F_GT);
    }
  }
  else if(s0->type == T_REAL){
    if(s1->type == T_NUMBER){
      ret->u.number = (s0->u.real > s1->u.number);
    }
    else if(s1->type == T_REAL){
      ret->u.number = (s0->u.real > s1->u.real);
    }
    else {
      bad_arg(2, F_GT);
    }
  }
  else if(s0->type != T_STRING){
    bad_arg(1, F_GT);
  }
  else if(s1->type != T_STRING){
    bad_arg(2, F_GT);
  }
  else {
    ret->u.number = (strcmp(s0->u.string, s1->u.string) > 0);
  }
}

void c_ge(svalue *ret, svalue *s0, svalue *s1) {
  ret->type = T_NUMBER;
  if(s0->type == T_NUMBER){
    if(s1->type == T_NUMBER){
      ret->u.number = (s0->u.number >= s1->u.number);
    }
    else if(s1->type == T_REAL){
      ret->u.number = (s0->u.number >= s1->u.real);
    }
    else {
      bad_arg(2, F_GE);
    }
  }
  else if(s0->type == T_REAL){
    if(s1->type == T_NUMBER){
      ret->u.number = (s0->u.real >= s1->u.number);
    }
    else if(s0->type == T_REAL){
      ret->u.number = (s0->u.real >= s1->u.real);
    }
    else {
      bad_arg(2, F_GE);
    }
  }
  else if(s0->type != T_STRING){
    bad_arg(1, F_GE);
  }
  else if(s1->type != T_STRING){
    bad_arg(2, F_GE);
  }
  else {
    ret->u.number = (strcmp(s0->u.string, s1->u.string) >= 0);
  }
}

void c_lt(svalue *ret, svalue *s0, svalue *s1) {
  ret->type = T_NUMBER;
  if (s0->type == T_NUMBER){
    if (s1->type == T_NUMBER){ /* optimize this case */
      ret->u.number = s0->u.number < s1->u.number;
    }
    else if(s1->type == T_REAL){
      ret->u.number = s0->u.number < s1->u.real;
    }
    else {
      bad_arg(2, F_LT);
    }
  }
  else if(s0->type == T_REAL){
    if(s1->type == T_NUMBER){
      ret->u.number = s0->u.real < s1->u.number;
    }
    else if(s1->type == T_REAL){
      ret->u.number = s0->u.real < s1->u.real;
    }
    else {
      bad_arg(2, F_LT);
    }
  }
  else if(s0->type != T_STRING){
    bad_arg(1, F_LT);
  }
  else if(s1->type != T_STRING){
    bad_arg(2, F_LT);
  }
  else {
    ret->u.number = (strcmp(s0->u.string, s1->u.string) < 0);
  }
}

void c_le(svalue *ret, svalue *s0, svalue *s1) {
  ret->type = T_NUMBER;
  if(s0->type == T_NUMBER){
    if(s1->type == T_NUMBER){
      ret->u.number = (s0->u.number <= s1->u.number);
    }
    else if(s1->type == T_REAL){
      ret->u.number = (s0->u.number <= s1->u.real);
    }
    else {
      bad_arg(2, F_LE);
    }
  }
  else if(s0->type == T_REAL){
    if(s1->type == T_NUMBER){
      ret->u.number = (s0->u.real <= s1->u.number);
    }
    else if(s1->type == T_REAL){
      ret->u.number = (s0->u.real <= s1->u.real);
    }
    else {
      bad_arg(2, F_LE);
    }
  }
  else if(s0->type != T_STRING){
    bad_arg(1, F_LE);
  }
  else if(s1->type != T_STRING){
    bad_arg(2, F_LE);
  }
  else {
    ret->u.number = (strcmp(s0->u.string, s1->u.string) <= 0);
  }
}

void c_lsh(svalue *ret, svalue *s0, svalue *s1) {
  if (s0->type != T_NUMBER)
    bad_arg(1, F_LSH);
  if (s1->type != T_NUMBER)
    bad_arg(1, F_LSH);
  C_NUMBER(ret, s0->u.number << s1->u.number);
}

void c_rsh(svalue *ret, svalue *s0, svalue *s1) {
  if (s0->type != T_NUMBER)
    bad_arg(1, F_RSH);
  if (s1->type != T_NUMBER)
    bad_arg(2, F_RSH);
  C_NUMBER(ret, s0->u.number >> s1->u.number);
}

void c_add(svalue *ret, svalue *s0, svalue *s1) {
  switch(s0->type){
  case T_BUFFER:
    if (s1->type != T_BUFFER) {
      error("Bad type argument to +. %s %s\n",
	    type_name(s0->type), type_name(s1->type));
    } else {
      struct buffer *b;
      
      b = allocate_buffer(s0->u.buf->size + s1->u.buf->size);
      memcpy(b->item, s0->u.buf->item, s0->u.buf->size);
      memcpy(b->item + s0->u.buf->size, s1->u.buf->item,
	     s1->u.buf->size);
      b->ref--;
      C_BUFFER(ret,b);
    }
    break;
  case T_REAL:
    {
      switch(s1->type)
	{
	case T_NUMBER:
	  C_REAL(ret,s1->u.number + s0->u.real);
	  break;
	case T_REAL:
	  C_REAL(ret,s1->u.real + s0->u.real);
	  break;
	case T_STRING:
	  {
	    char buff[100], *res;
	    int len,r;
	    
	    sprintf(buff, "%f", s0->u.real);
	    len = SVALUE_STRLEN(s1) + (r=strlen(buff)) + 1;
	    res = DXALLOC(len, 36, "f_add: 3");
	    strcpy(res, buff);
	    strcpy(res+r, s1->u.string);
	    C_MALLOCED_STRING(ret,res);
	    break;
	  }
	}
    }
  case T_STRING:
    {
      switch(s1->type)
	{
	case T_STRING:
	  {
	    char *res;
	    int r = SVALUE_STRLEN(s0);
	    int len = r + SVALUE_STRLEN(s1) + 1;
	    
	    res = DXALLOC(len, 34, "c_add: 1");
	    (void)strcpy(res, s0->u.string);
	    (void)strcpy(res + r, s1->u.string);
	    C_MALLOCED_STRING(ret,res);
	    break;
	  } 
	case T_NUMBER:
	  {
	    char buff[20];
	    char *res;
	    int r,len;
	    
	    sprintf(buff, "%d", s1->u.number);
	    len = (r=SVALUE_STRLEN(s0)) + strlen(buff) + 1;
	    
	    res = DXALLOC(len, 35, "f_add: 2");
	    strcpy(res, s0->u.string);
	    strcpy(res+r, buff);
	    C_MALLOCED_STRING(ret,res);
	    break;
	  }
	case T_REAL:
	  {
	    char buff[25];
	    char *res;
	    int r,len;
	    
	    sprintf(buff, "%f", s1->u.real);
	    len = (r=SVALUE_STRLEN(s0)) + strlen(buff) + 1;
	    
	    res = DXALLOC(len, 35, "f_add: 2");
	    strcpy(res, s0->u.string);
	    strcpy(res+r, buff);
	    C_MALLOCED_STRING(ret,res);
	    break;
	  }
	default: 
	  error("Bad type argument to +. %s %s\n",
		type_name(s0->type), type_name(s1->type));
	}
      break;
    } 
  case T_NUMBER:
    {
      switch(s1->type)
	{
	case T_NUMBER:
	  C_NUMBER(ret,s1->u.number + s0->u.number);
	  break;
	case T_REAL:
	  C_REAL(ret,s1->u.real + s0->u.number);
	  break;
	case T_STRING:
	  {
	    char buff[20], *res;
	    int r,len;
	    
	    sprintf(buff, "%d", s0->u.number);
	    len = SVALUE_STRLEN(s1) + (r=strlen(buff)) + 1;
	    res = DXALLOC(len, 36, "f_add: 3");
	    strcpy(res, buff);
	    strcpy(res+r, s1->u.string);
	    C_MALLOCED_STRING(ret,res);
	    break;
	  }
	default: 
	  error("Bad type argument to +. %s %s\n",
		type_name(s0->type), type_name(s1->type));
	}
      break;
    }
  case T_POINTER:
    {
      if (s1->type != T_POINTER) {
	error("Bad type argument to +. %s %s\n",
	      type_name(s0->type), type_name(s1->type));
      } else {
	struct vector *vec;
	
	/* add_array frees the vectors; we don't want that */
	s0->u.vec->ref++; s1->u.vec->ref++;
	vec = add_array(s0->u.vec, s1->u.vec);
	C_VECTOR(ret,vec);
	vec->ref--;
	break;
      }
      break;
    } 
  case T_MAPPING:
    if (s1->type == T_MAPPING) {
      struct mapping *map;
      
      map = add_mapping(s0->u.map, s1->u.map);
      C_MAPPING(ret,map);
      map->ref--;
    }
    else {
      error("Bad type argument to +. %s %s\n",
	    type_name(s0->type), type_name(s1->type));
    }
    break;
  default: 
    error("Bad type argument to +. %s %s\n",
	  type_name(s0->type), type_name(s1->type));
  }
}

void c_subtract(svalue *ret, svalue *s0, svalue *s1) {
  if(s0->type == T_NUMBER){
    if(s1->type == T_NUMBER){
      C_NUMBER(ret,s0->u.number - s1->u.number);
    }
    else if(s1->type == T_REAL){
      C_REAL(ret,s0->u.number - s1->u.real);
    }
    else {
      error("Bad right type to -\n");
    }
  }
  else if(s0->type == T_REAL){
    if(s1->type == T_NUMBER){
      C_REAL(ret,s0->u.real - s1->u.number);
    }
    else if(s1->type == T_REAL){
      C_REAL(ret,s0->u.real - s1->u.real);
    }
    else {
      error("Bad right type to -\n");
    }
  }
  else if(s0->type == T_POINTER){
    if(s1->type == T_POINTER){
      extern struct vector *subtract_array
	PROT((struct vector*,struct vector*));
      struct vector *v, *w;
      
      v = slice_array(s1->u.vec, 0, s1->u.vec->size-1 );
      
      /* subtract_array already takes care of destructed objects */
      w = subtract_array(s0->u.vec, v);
      free_vector(v);
      C_VECTOR(ret,w);
      ret->u.vec->ref--;
      return;
    }
    else {
      error("Bad right type to -\n");
    }
  }
  else {
    error("Bad left type to -\n");
  }
}

void c_multiply(svalue *ret, svalue *s0, svalue *s1) {
  if((s0->type != s1->type)
     && ((s0->type != T_NUMBER) || (s1->type != T_REAL))
     && ((s0->type != T_REAL) || (s1->type != T_NUMBER)))
    bad_arg(1, F_MULTIPLY);
  
  if(s0->type == T_NUMBER){
    if(s1->type == T_REAL){
      C_REAL(ret,s0->u.number * s1->u.real);
    } else {
      C_NUMBER(ret,s0->u.number * s1->u.number);
    }
    return;
  } else if (s0->type == T_REAL){
    if(s1->type == T_NUMBER){
      C_REAL(ret,s0->u.real * s1->u.number);
    } else {
      C_REAL(ret,s0->u.real * s1->u.real);
    }
    return;
  } else if(s0->type == T_MAPPING){
    struct mapping *m;
    
    m = compose_mapping(s0->u.map, s1->u.map);
    C_MAPPING(ret,m);
    return;
  }
  bad_arg(2, F_MULTIPLY);
}

void c_mod(svalue *ret, svalue *s0, svalue *s1) {
  if (s0->type != T_NUMBER)
    bad_arg(1, F_MOD);
  if (s1->type != T_NUMBER)
    bad_arg(2, F_MOD);
  if (s1->u.number == 0)
    error("Modulus by zero.\n");
  C_NUMBER(ret, (s0->u.number % s1->u.number));
}

void c_divide(svalue *ret, svalue *s0, svalue *s1) {
  if ((s0->type != T_NUMBER) && (s0->type != T_REAL))
    bad_arg(1, F_DIVIDE);
  if ((s1->type != T_NUMBER) && (s1->type != T_REAL))
    bad_arg(2, F_DIVIDE);
  if ((s1->type == T_NUMBER) && (s1->u.number == 0))
    error("Division by zero\n");
  if ((s1->type == T_REAL) && (s1->u.real == 0.0))
    error("Division by zero\n");
  if (s1->type == T_NUMBER) {
    if (s0->type == T_NUMBER) {
      C_NUMBER(ret,s0->u.number / s1->u.number);
      return;
    } else { /* T_REAL */
      C_REAL(ret,s0->u.real / s1->u.number);
      return;
    }
  } else { /* T_REAL */
    if (s0->type == T_REAL) {
      C_REAL(ret,s0->u.real / s1->u.real);
      return;
    } else {
      C_REAL(ret,s0->u.number / s1->u.real);
      return;
    }
  }
}

void c_compl(svalue *ret, svalue *s0) {
  if (s0->type != T_NUMBER)
    error("Bad argument to ~\n");
  C_NUMBER(ret, ~s0->u.number);
}

void c_negate(svalue *ret, svalue *s0) {
  if (s0->type == T_NUMBER)
    C_NUMBER(ret,-s0->u.number);
  else if (s0->type == T_REAL)
    C_REAL(ret,-s0->u.real);
  else
    error("Bad argument to unary minus\n");
}

void eval_opcode(int instruction,
		 svalue *ret, 
		 svalue *s0, svalue *s1, svalue *s2, svalue *s3 ) {
  int i;

#ifdef TRACE
    if (TRACEP(TRACE_LPC_EXEC)) {
      do_trace("LPC Exec ", get_f_name(instruction), "\n");
    }
#endif
    switch (instruction) {
    case I(F_PRE_INC) :
      if(s0->type == T_NUMBER){
        s0->u.number++;
        *ret = *s0;
        break;
      }
      else if(s0->type == T_REAL){
        s0->u.real++;
        *ret = *s0;
        break;
      }
      else {
        error("++ of non-numeric argument\n");
      }
      break;
    case I(F_INC) :
      if(s0->type == T_NUMBER){
	s0->u.number++;
	break;
      }
      else if(s0->type == T_REAL){
	s0->u.real++;
	break;
      }
      else {
	error("++ of non-numeric argument\n");
      }
      break;
    case I(F_POST_INC) :
      if(s0->type == T_NUMBER){
        *ret = *s0;
        s0->u.number++;
        break;
      }
      else if(s0->type == T_REAL){
	*ret = *s0;
	s0->u.real++;
        break;
      }
      else {
        error("++ of non-numeric argument\n");
      }
      break;
    case I(F_PRE_DEC) :
      if(s0->type == T_NUMBER){
        s0->u.number--;
	*ret = *s0;
        break;
      }
      else if(s0->type == T_REAL){
        s0->u.real--;
	*ret = *s0;
        break;
      }
      else {
        error("-- of non-numeric argument\n");
      }
      break;
    case I(F_DEC) :
      if(s0->type == T_NUMBER){
	s0->u.number--;
      }
      else if(s0->type == T_REAL){
	s0->u.real--;
      }
      else {
        error("-- of non-numeric argument\n");
      }
      break;
    case I(F_POST_DEC) :
      if(s0->type == T_NUMBER){
        *ret = *s0;
        s0->u.number--;
        break;
      }
      else if(s0->type == T_REAL){
	*ret = *s0;
	s0->u.real--;
        break;
      }
      else {
        error("-- of non-numeric argument\n");
      }
      break;
    case I(F_VOID_ADD_EQ) :
    case I(F_ADD_EQ) :
      {
	switch(s0->type) {
	case T_STRING:
	  {
	    char *new_str = 0;
	    
	    if(s1->type == T_STRING){
	      int l = SVALUE_STRLEN(s0);
	      int len = l + strlen(s1->u.string) + 1;
	      
	      new_str = DXALLOC(len, 45, "f_add_eq: 1");
	      strcpy(new_str, s0->u.string);
	      strcpy(new_str + l, s1->u.string);
	    }
	    else if (s1->type == T_NUMBER){
	      char buff[20];
	      int len,l;
	      
	      sprintf(buff, "%d", s1->u.number);
	      len = (l=SVALUE_STRLEN(s0)) + strlen(buff) + 1;
	      new_str = DXALLOC(len, 46, "f_add_eq: 2");
	      strcpy(new_str, s0->u.string);
	      strcpy(new_str+l, buff);
	    }
            else if (s1->type == T_REAL){
              char buff[20];
              int len,l;
	      
              sprintf(buff, "%f", s1->u.real);
              len = (l=SVALUE_STRLEN(s0)) + strlen(buff) + 1;
              new_str = DXALLOC(len, 46, "f_add_eq: 2");
              strcpy(new_str, s0->u.string);
              strcpy(new_str+l, buff);
            }
	    else {
	      bad_arg(2, instruction);
	    }
	    free_string_svalue(s0); /* free the LHS */
	    s0->subtype = STRING_MALLOC;
	    s0->u.string = new_str;
	  }
	  break;
	case T_NUMBER:
	  if(s1->type == T_NUMBER){
	    s0->u.number += s1->u.number;
	  } else
	    if(s1->type == T_REAL){
	      s0->u.number += s1->u.real;
	    }
	    else {
	      error("Bad type number to rhs +=.\n");
	    }
	  break;
        case T_REAL:
	  if(s1->type == T_NUMBER){
            s0->u.real += s1->u.number;
          }
          if(s1->type == T_REAL){
            s0->u.real += s1->u.real;
          }
          else {
            error("Bad type number to rhs +=.\n");
          }
          break;
	case T_BUFFER:
	  if (s1->type != T_BUFFER) {
	    bad_arg(2, instruction);
	  } else {
	    struct buffer *b;
	    
	    b = allocate_buffer(s0->u.buf->size + s1->u.buf->size);
	    memcpy(b->item, s0->u.buf->item, s0->u.buf->size);
	    memcpy(b->item + s0->u.buf->size, s1->u.buf->item,
		   s1->u.buf->size);
	    free_buffer(s0->u.buf);
	    s0->u.buf = b;
	  }
	  break;
	case T_POINTER:
	  if (s1->type != T_POINTER)
	    bad_arg(2, instruction);
	  else {
	    struct vector *v;
	    
	    /* ok to free the RHS */
	    s1->u.vec->ref++;
	    v = add_array(s0->u.vec, s1->u.vec);
	    s0->u.vec = v;
	  }
	  break;
	case T_MAPPING:
	  if (s1->type != T_MAPPING)
	    bad_arg(2, instruction);
	  else {
	    absorb_mapping(s0->u.map, s1->u.map);
	  }
	  break;
	default:
	  bad_arg(1, instruction);
	}
	/* if (void)add_eq then no need to produce an rvalue */
	if (instruction == F_ADD_EQ) { /* not void add_eq */
	  assign_svalue_no_free(ret, s0);
	}
      }
      break;
    case I(F_AND_EQ) :
      if (s0->type != T_NUMBER)
	error("Bad left type to &=\n");
      if (s1->type != T_NUMBER)
	error("Bad right type to &=\n");
      C_NUMBER(ret, s0->u.number &= s1->u.number);
      break;
    case I(F_FUNCTION_CONSTRUCTOR) :
      c_function_constructor(ret, s0, s1);
      break;
    case I(F_ASSIGN) :
      if (s0->type == T_LVALUE_BYTE) {
	if (s1->type != T_NUMBER) {
	  *s0->u.lvalue_byte = 0;
	} else {
	  *s0->u.lvalue_byte = (s1->u.number & 0xff);
	}
      } else {
	assign_svalue(s0, s1);
      }
      assign_svalue_no_free(ret, s1);
      break;
    case I(F_VOID_ASSIGN) :
      if (s0->type == T_LVALUE_BYTE) {
	if (s1->type != T_NUMBER) {
	  *s0->u.lvalue_byte = 0;
	} else {
	  *s0->u.lvalue_byte = (s1->u.number & 0xff);
	}
      } else {
	/* No T_INVALID here; LPC->C uses C_ASSIGN_FROM_STACK instead */
	assign_svalue(s0,s1);
      }
      break;
    case I(F_DIV_EQ) :
      if(s0->type == T_NUMBER){
	if(s1->type == T_NUMBER){
	  if(s1->u.number == 0)
	    error("Division by 0nn\n");
	  C_NUMBER(ret, s0->u.number /= s1->u.number);
	}
	else if (s1->type == T_REAL) {
	  if(s1->u.real == 0.)
	    error("Division by 0.nr\n");
	  C_NUMBER(ret, s0->u.number /= s1->u.real);
	}
	else {
	  error("Bad right type to /=\n");
	}
      }
      else if(s0->type == T_REAL){
	if(s1->type == T_NUMBER){
	  if(s1->u.number == 0)
	    error("Division by 0rn\n");
	  C_REAL(ret, s0->u.real /= s1->u.number);
	}
	else if (s1->type == T_REAL) {
	  if(s1->u.real == 0.)
	    error("Division by 0.rr\n");
	  C_REAL(ret, s0->u.real /= s1->u.real);
	}
	else {
	  error("Bad right type to /=\n");
	}
      }
      else {
	error("Bad left type to /=\n");
      }
      break;
    case I(F_LSH_EQ) :
      if (s0->type != T_NUMBER)
	error("Bad left type to <<=\n");
      if (s1->type != T_NUMBER)
	error("Bad right type to <<=\n");
      C_NUMBER(ret, s0->u.number <<= s1->u.number);
      break;
    case I(F_MOD_EQ) :
      if (s0->type != T_NUMBER)
	error("Bad left type to %=\n");
      if (s1->type != T_NUMBER)
	error("Bad right type to %=\n");
      if (s1->u.number == 0)
	error("Division by 0\n");
      C_NUMBER(ret, s0->u.number %= s1->u.number);
      break;
    case I(F_MULT_EQ) :
      if(s0->type == T_NUMBER){
	if(s1->type == T_NUMBER){
	  C_NUMBER(ret, s0->u.number *= s1->u.number);
	}
	else if(s1->type == T_REAL){
	  C_NUMBER(ret, s0->u.number *= s1->u.real);
	}
	else {
	  error("Bad right type to *=\n");
	}
      }
      else if(s0->type == T_REAL){
	if(s1->type == T_NUMBER){
	  C_NUMBER(ret, s0->u.real *= s1->u.number);
	}
	else if(s1->type == T_REAL){
	  C_NUMBER(ret, s0->u.real *= s1->u.real);
	}
	else {
	  error("Bad right type to *=\n");
	}
      }
      else if(s0->type == T_MAPPING){
	if(s1->type == T_MAPPING){
	  struct mapping *m;
	  
	  m = compose_mapping(s0->u.map, s1->u.map);
	  C_MAPPING(ret,m);
	  assign_svalue(s0, ret);
	}
	else {
	  error("Bad right type to *=\n");
	}
      }
      else {
	error("Bad left type to *=\n");
      }
      break;
    case I(F_NOT) :
      if (s0->type == T_NUMBER && s0->u.number == 0)
	C_NUMBER(ret,1);
      else
	*ret = const0;
      break;
    case I(F_OR_EQ) :
      if (s0->type != T_NUMBER)
	error("Bad left type to |=\n");
      if (s1->type != T_NUMBER)
	error("Bad right type to |=\n");
      C_NUMBER(ret, s0->u.number |= s1->u.number);
      break;
    case I(F_RSH_EQ) :
      if (s0->type != T_NUMBER)
	error("Bad left type to >>=\n");
      if (s1->type != T_NUMBER)
	error("Bad right type to >>=\n");
      C_NUMBER(ret, s0->u.number >>= s1->u.number);
      break;
    case I(F_SUB_EQ) :
      switch(s0->type){
      case T_NUMBER:
	if(s1->type == T_NUMBER){
	  C_NUMBER(ret, s0->u.number -= s1->u.number);
	}
	else if(s1->type == T_REAL){
	  C_NUMBER(ret, s0->u.number -= s1->u.real);
	}
	else {
	  error("Bad type number to rhs -=\n");
	}
	break;
      case T_REAL:
	if(s1->type == T_NUMBER){
	  C_REAL(ret, s0->u.real -= s1->u.number);
	}
	else if(s1->type == T_REAL){
	  C_REAL(ret, s0->u.real -= s1->u.real);
	}
	else {
	  error("Bad type number to rhs -=\n");
	}
	break;
      case T_POINTER:
	if(s1->type == T_POINTER){
	  struct vector *subtract_array PROT((struct vector*,struct vector*));
	  struct vector *v, *w;
	  
	  v = slice_array(s1->u.vec, 0, s1->u.vec->size-1 );
	  
	  w = subtract_array(s0->u.vec, v);
	  free_vector(s0->u.vec);
	  free_vector(v);
	  s0->u.vec = w;
	  assign_svalue_no_free(ret, s0);
	}
	else {
	  error("Bad type number to rhs -=\n");
	}
	break;
      default:
	error("Bad left type to -=\n");
      }
      break;
    case I(F_XOR_EQ) :
      if (s0->type != T_NUMBER)
	error("Bad left type to ^=\n");
      if (s1->type != T_NUMBER)
	error("Bad right type to ^=\n");
      C_NUMBER(ret, s0->u.number ^= s1->u.number);
      break;
    default:
      dump_trace(1);
      fatal("Undefined instruction %s in eval_opcode (%d)\n",
	    get_f_name(instruction), instruction);
      return;
    }
}

#endif


