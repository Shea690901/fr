#include <stdio.h>
#include <string.h>
#ifdef sun
#include <alloca.h>
#endif
#include <sys/types.h>
#include "config.h"
#include "lint.h"
#include "interpret.h"
#include "object.h"
#include "regexp.h"
#include "exec.h"
#include "sent.h"
#include "debug.h"
#include "comm.h"

/*
 * This file contains functions used to manipulate arrays.
 * Some of them are connected to efuns, and some are only used internally
 * by the MudOS driver.
 */

struct vector *order_alist PROT((struct vector *));

extern int d_flag;
extern int max_array_size;

int num_arrays;
int total_array_size;

/*
 * Make an empty vector for everyone to use, never to be deallocated.
 * It is cheaper to reuse it, than to use MALLOC() and allocate.
 */
struct vector null_vector = {
    0,	/* size */
    1	/* Ref count, which will ensure that it will never be deallocated */
};

INLINE struct vector *
null_array()
{
	null_vector.ref++;
	return &null_vector;
}

/*
 * Allocate an array of size 'n'.
 */
struct vector *allocate_array(n)
    int n;
{
    extern struct svalue const0;
    int i;
    struct vector *p;

    if (n < 0 || n > max_array_size)
	error("Illegal array size.\n");
    if (n == 0) {
		return null_array();
    }
    num_arrays++;
    total_array_size += sizeof (struct vector) + sizeof (struct svalue) *
	(n-1);
    p = ALLOC_VECTOR(n);
    p->ref = 1;
    p->size = n;
#ifndef NO_MUDLIB_STATS
    if (current_object) {
      assign_stats(&p->stats, current_object);
      add_array_size (&p->stats, n);
    } else {
      null_stats (&p->stats);
    }
#endif
    for (i = n; i--; )
	p->item[i] = const0;
    return p;
}

void free_vector(p)
    struct vector *p;
{
    int i;

	p->ref--;
	/* don't keep track of the null vector since many muds reference it enough
	   times to overflow the ref count which is a short int.
	*/
	if ((p->ref > 0) || (p == &null_vector)) {
		return;
	}

        for (i = p->size; i--; )
		free_svalue(&p->item[i]);
#ifndef NO_MUDLIB_STATS
	add_array_size (&p->stats, - p->size);
#endif
	num_arrays--;
	total_array_size -= sizeof (struct vector) + sizeof (struct svalue) *
		(p->size-1);
	FREE((char *)p);
}

struct vector *shrink_array(p, n)
    struct vector *p;
    int n;
{
    if (n <= p->size >> 1) {
	struct vector *res;

	res = slice_array(p, 0, n-1);
	free_vector(p);
	return res;
    }
    total_array_size += sizeof (struct svalue) * (n - p->size);
#ifndef NO_MUDLIB_STATS
    add_array_size (&p->stats, n - p->size);
#endif
    p->size = n;
    return p;
}

struct vector *explode_string(str, del)
    char *str, *del;
{
    char *p, *beg, *lastdel = (char *)NULL;
    int num, len, j, slen, limit;
    struct vector *ret;
    char *buff, *tmp;

    len = strlen(del);
    slen = strlen(str);

    if (!slen)
        return null_array();

    /* return an array of length strlen(str) -w- one character per element */
    if (len == 0) {
        if (slen > max_array_size) {
            slen = max_array_size;
        }
        ret = allocate_array(slen);
        for (j = 0; j < slen; j++) {
            ret->item[j].type = T_STRING;
            ret->item[j].subtype = STRING_MALLOC;
            tmp = (char *)DXALLOC(2, 6, "explode_string: tmp");
            tmp[0] = str[j];
            tmp[1] = '\0';
            ret->item[j].u.string = tmp;
        }
        return ret;
    }

    if (len == 1) {
        char delimeter;

        delimeter = *del;

        /*
         *Skip leading 'del' strings, if any.
         */
        while(*str == delimeter) {
            str++;
            slen--;
            if (str[0] == '\0') {
                return null_array();
            }
#ifdef SANE_EXPLODE_STRING
            break;
#endif
        }

        /*
         * Find number of occurences of the delimiter 'del'.
         */
        for (p=str, num=0; *p; ) {
            if (*p == delimeter) {
                num++;
                lastdel = p;
            }
            p++;
        }

        /*
         * Compute number of array items. It is either number of delimiters,
         * or, one more.
         */
        if ((slen == 1) || (lastdel != (str + slen - 1))) {
            num++;
        }
        if (num > max_array_size) {
            num = max_array_size;
        }
        ret = allocate_array(num);
        limit = max_array_size - 1; /* extra element can be added after loop */
        for (p=str, beg = str, num=0; *p && (num < limit); ) {
            if (*p == delimeter) {
                if (num >= ret->size) {
                    fatal("Index out of bounds in explode!\n");
                }

                /* free_svalue(&ret->item[num]); Not needed for new array */
                ret->item[num].type = T_STRING;
                ret->item[num].subtype = STRING_MALLOC;
                ret->item[num].u.string = buff =
                      DXALLOC(p - beg + 1, 7, "explode_string: buff");

                strncpy(buff, beg, p - beg);
                buff[p-beg] = '\0';
                num++;
                beg = ++p;
            } else {
                p++;
            }
        }

        /* Copy last occurence, if there was not a 'del' at the end. */
        if (*beg != '\0') {
            /* free_svalue(&ret->item[num]); */
            ret->item[num].type = T_STRING;
            ret->item[num].subtype = STRING_MALLOC;
            ret->item[num].u.string = string_copy(beg);
        }
        return ret;
    } /* len == 1 */

    /*
     *Skip leading 'del' strings, if any.
     */
    while(strncmp(str, del, len) == 0) {
        str += len;
        slen -= len;
        if (str[0] == '\0') {
            return null_array();
        }
#ifdef SANE_EXPLODE_STRING
        break;
#endif
    }

    /*
     * Find number of occurences of the delimiter 'del'.
     */
    for (p=str, num=0; *p; ) {
        if (strncmp(p, del, len) == 0) {
            num++;
            lastdel = p;
            p += len;
        } else {
            p++;
        }
    }

    /*
     * Compute number of array items. It is either number of delimiters,
     * or, one more.
     */
    if ((slen <= len) || (lastdel != (str + slen - len))) {
        num++;
    }
    if (num > max_array_size) {
        num = max_array_size;
    }

    ret = allocate_array(num);
    limit = max_array_size - 1; /* extra element can be added after loop */
    for (p=str, beg = str, num=0; *p && (num < limit); ) {
        if (strncmp(p, del, len) == 0) {
            if (num >= ret->size)
                fatal("Index out of bounds in explode!\n");

            /* free_svalue(&ret->item[num]); Not needed for new array */
            ret->item[num].type = T_STRING;
            ret->item[num].subtype = STRING_MALLOC;
            ret->item[num].u.string = buff =
                  DXALLOC(p - beg + 1, 7, "explode_string: buff");

            strncpy(buff, beg, p - beg);
            buff[p-beg] = '\0';
            num++;
            beg = p + len;
            p = beg;
        } else {
            p++;
        }
    }

    /* Copy last occurence, if there was not a 'del' at the end. */
    if (*beg != '\0') {
        /* free_svalue(&ret->item[num]); */
        ret->item[num].type = T_STRING;
        ret->item[num].subtype = STRING_MALLOC;
        ret->item[num].u.string = string_copy(beg);
    }
    return ret;
}

char *implode_string(arr, del)
    struct vector *arr;
    char *del;
{
    int size, i, num;
    char *p, *q;

    for (i = arr->size, size = 0, num = 0; i--; ) {
	if (arr->item[i].type == T_STRING) {
	    size += strlen(arr->item[i].u.string);
	    num++;
	}
    }
    if (num == 0)
	return string_copy("");
    p = DXALLOC(size + (num-1) * strlen(del) + 1, 8, "implode_string: p");
    q = p;
    p[0] = '\0';
    for (i=0, size=0, num=0; i < arr->size; i++) {
	if (arr->item[i].type == T_STRING) {
	    if (num > 0) {
		strcpy(p, del);
		p += strlen(del);
	    }
	    strcpy(p, arr->item[i].u.string);
	    p += strlen(arr->item[i].u.string);
	    num++;
	}
    }
    return q;
}

struct vector *
users()
{
	register struct object *ob;
	extern int num_user, num_hidden; /* set by comm1.c */
	extern struct interactive *all_users[MAX_USERS];
	int i, j;
	int display_hidden = 0;
	struct vector *ret;

	if (num_hidden > 0) {
		if (current_object->flags & O_HIDDEN) {
			display_hidden = 1;
		} else {
			display_hidden = valid_hide(current_object);
		}
	}

	ret = allocate_array(num_user - (display_hidden ? 0 : num_hidden));
        for (i = j = 0; i < MAX_USERS; i++) {
		if (!all_users[i]) {
			continue;
		}
		ob = all_users[i]->ob;
		if (!display_hidden && (ob->flags & O_HIDDEN)) {
			continue;
		}
		ret->item[j].type = T_OBJECT;
		ret->item[j].u.ob = ob;
		add_ref(ob, "users");
		j++;
	}
	return ret;
}

/*
 * Slice of an array.
 */
struct vector *slice_array(p,from,to)
    struct vector *p;
    int from;
    int to;
{
    struct vector *d;
    int cnt;
    
    if (from < 0)
         from = 0;
    if (from >= p->size)
	return null_array(); /* Slice starts above array */
    if (to >= p->size)
	to = p->size-1;
    if (to < from)
	return null_array(); 
    
    d = allocate_array(to-from+1);
    for (cnt=from;cnt<=to;cnt++) 
	assign_svalue_no_free (&d->item[cnt-from], &p->item[cnt]);
    
    return d;
}


#ifndef AURORA
struct vector *commands(ob)
    struct object *ob;
{
    struct sentence *s;
    struct vector *v;
    int cnt = 0;

    for (s = ob->sent; s && (cnt < max_array_size); s = s->next) {
       if (s->verb) cnt++;
    }
    v = allocate_array(cnt);
    cnt = 0;
    for (s = ob->sent; s && cnt < max_array_size; s = s->next) {
       struct vector *p;

       if (!s->verb) continue;
       v->item[cnt].type = T_POINTER;
       v->item[cnt].u.vec = p = allocate_array(3);
       p->item[0].type = T_STRING;
       p->item[0].u.string = ref_string(s->verb);  /* the verb is shared */
       p->item[0].subtype = STRING_SHARED;
       p->item[1].type = T_NUMBER;
       p->item[1].u.number = s->flags;
       p->item[2].type = T_OBJECT;
       p->item[2].u.ob = s->ob;
       add_ref(s->ob, "commands");
       cnt++;
    }
    return v;
}
#endif

/* EFUN: filter_array
   
   Runs all elements of an array through ob->func()
   and returns an array holding those elements that ob->func
   returned 1 for.
   */
struct vector *filter (p, func, ob, extra)
    struct vector *p;
    char *func;
    struct object *ob;
    struct svalue *extra;
{
    extern struct svalue *sp;
    struct vector *r;
    struct svalue *v;
    char *flags;
    int cnt,res;

    res=0;
    r=0;
    if ( !func || !ob || (ob->flags & O_DESTRUCTED)) {
	if (d_flag) debug_message ("filter: invalid agument\n");
	return 0;
    }
    if (p->size<1)
	return null_array();

    flags=DXALLOC(p->size+1, 9, "filter: flags");

    push_malloced_string(flags);

    for (cnt=0;cnt<p->size;cnt++) {
	flags[cnt]=0;
	push_svalue(&p->item[cnt]);
	if (extra) {
	    push_svalue(extra);
	    v = apply (func, ob, 2);
	} else {
	    v = apply (func, ob, 1);
	}
	if (!IS_ZERO(v)) { flags[cnt]=1; res++; }
    }
    r = allocate_array(res);
    if (res) {
        while (cnt--) {
	    if (flags[cnt]) 
		assign_svalue_no_free (&r->item[--res], &p->item[cnt]);
	}
    }

    FREE(flags);
    sp--;

    return r;
}

/* Unique maker
   
   These routines takes an array of objects and calls the function 'func'
   in them. The return values are used to decide which of the objects are
   unique. Then an array on the below form are returned:
   
   ({
   ({Same1:1, Same1:2, Same1:3, .... Same1:N }),
   ({Same2:1, Same2:2, Same2:3, .... Same2:N }),
   ({Same3:1, Same3:2, Same3:3, .... Same3:N }),
   ....
   ....
   ({SameM:1, SameM:2, SameM:3, .... SameM:N }),
   })
   i.e an array of arrays consisting of lists of objectpointers
   to all the nonunique objects for each unique set of objects.
   
   The basic purpose of this routine is to speed up the preparing of the
   array used for describing.
   
   */

struct unique
{
    int count;
    struct svalue *val;
    struct svalue mark;
    struct unique *same;
    struct unique *next;
};

int sameval(arg1,arg2)  /* nonstatic, is used in mappings too */
    struct svalue *arg1;
    struct svalue *arg2;
{
    if (!arg1 || !arg2) return 0;

    if (arg1->type == arg2->type) {
        switch (arg1->type) {
            case T_NUMBER:
                return arg1->u.number == arg2->u.number;
            case T_POINTER:
                return arg1->u.vec == arg2->u.vec;
            case T_STRING:
                return !strcmp(arg1->u.string, arg2->u.string);
            case T_OBJECT:
                return arg1->u.ob == arg2->u.ob;
            case T_MAPPING:
                return arg1->u.map == arg2->u.map;
        }
    }

    return 0;
}


static int put_in(ulist,marker,elem)
    struct unique **ulist;
    struct svalue *marker;
    struct svalue *elem;
{
    struct unique *llink,*slink,*tlink;
    int cnt,fixed;
    
    llink= *ulist;
    cnt=0; fixed=0;
    while (llink) {
	if ((!fixed) && (sameval(marker,&(llink->mark)))) {
	    for (tlink=llink;tlink->same;tlink=tlink->same)
	        (tlink->count)++;
	    (tlink->count)++;
	    slink = (struct unique *) DXALLOC(sizeof(struct unique),10,
			"put_in: slink");
	    slink->count = 1;
	    assign_svalue_no_free(&slink->mark,marker);
	    slink->val = elem;
	    slink->same = 0;
	    slink->next = 0;
	    tlink->same = slink;
	    fixed=1; /* We want the size of the list so do not break here */
	}
	llink=llink->next; cnt++;
    }
    if (fixed) return cnt;
    llink = (struct unique *) DXALLOC(sizeof(struct unique), 11,
		"put_in: llink");
    llink->count = 1;
    assign_svalue_no_free(&llink->mark,marker);
    llink->val = elem;
    llink->same = 0;
    llink->next = *ulist;
    *ulist = llink;
    return cnt+1;
}


struct vector *
make_unique(arr,func,skipnum)
    struct vector *arr;
    char *func;
    struct svalue *skipnum;
{
    struct svalue *v;
    struct vector *res,*ret;
    struct unique *head,*nxt,*nxt2;
    
    int cnt,ant,cnt2;
    
    if (arr->size < 1)
	return null_array();

    head = 0; ant=0;
    for(cnt=0;cnt<arr->size;cnt++)
        if (arr->item[cnt].type == T_OBJECT) {
	    v = apply(func,arr->item[cnt].u.ob,0);
	    if (v && ((v->type!=skipnum->type) || !sameval(v,skipnum)))
	        ant=put_in(&head,v,&(arr->item[cnt]));
    }
    ret = allocate_array(ant);
    
    for (cnt=ant-1;cnt>=0;cnt--) { /* Reverse to compensate put_in */
	ret->item[cnt].type = T_POINTER;
	ret->item[cnt].u.vec = res = allocate_array(head->count);
	nxt2 = head;
	head = head->next;
	cnt2 = 0;
	while (nxt2) {
	    assign_svalue_no_free (&res->item[cnt2++], nxt2->val);
	    free_svalue(&nxt2->mark);
	    nxt = nxt2->same;
	    FREE((char *) nxt2);
	    nxt2 = nxt;
	}
	if (!head) 
	    break; /* It shouldn't but, to avoid skydive just in case */
    }
    return ret;
}

/*
 * End of Unique maker
 *************************
 */

/* Concatenation of two arrays into one
 */
struct vector *add_array(p,r)
    struct vector *p, *r;
{
    int cnt, res;
    struct vector *d;   /* destination */

    res = p->size + r->size;
    if (res < 0 || res > max_array_size)
        error("Illegal array size.\n");
    if (res == 0)
        return null_array();

    /* transfer svalues for ref 1 target array */
    if (p->ref == 1) {
        /*
         * realloc(p) to try extending block; this will save an
         * allocate_array(), copying the svalues over, and free()'ing p
         */
        d = (struct vector *)DREALLOC(p, sizeof(struct vector) +
              sizeof(struct svalue) * (res - 1), 121, "add_array()");
        if (!d)
            fatal("Out of memory.\n");

        total_array_size += sizeof (struct svalue) * (r->size);
        d->ref = 1;
        d->size = res;

#ifndef NO_MUDLIB_STATS
        /* mudlib_stats stuff */
        if (current_object) {
            assign_stats(&d->stats, current_object);
            add_array_size (&d->stats, r->size);
        } else {
            null_stats (&d->stats);
        }
#endif
    } else {
        d = allocate_array(res);

        for (cnt = p->size; cnt--; )
            assign_svalue_no_free(&d->item[cnt], &p->item[cnt]);
        free_vector(p);
    }

    /* transfer svalues from ref 1 source array */
    if (r->ref == 1) {
        for (cnt = r->size; cnt--; )
            d->item[--res] = r->item[cnt];
#ifndef NO_MUDLIB_STATS
        add_array_size (&r->stats, - r->size);
#endif
        num_arrays--;
        total_array_size -= sizeof (struct vector) +
              sizeof (struct svalue) *(r->size-1);
        FREE((char *)r);
    } else {
        for (cnt = r->size; cnt--; )
            assign_svalue_no_free(&d->item[--res], &r->item[cnt]);
        free_vector(r);
    }
    
    return d;
}

/* fix subtract_array to use "cosequential processing" to make this
   operation order N instead of N^2 (discounting the cost of sorting which
   is being done anyway) -- Truilkan 1992/07/20
*/

struct vector *subtract_array(minuend, subtrahend)
    struct vector *minuend, *subtrahend;
{
    struct vector *order_alist PROT((struct vector *));
    struct vector *vtmpp;
    static struct vector vtmp = { 1, 1,
#ifdef DEBUG
				    1,
#endif
#ifndef NO_MUDLIB_STATS
                                  {(mudlib_stats_t *)NULL,
                                     (mudlib_stats_t *)NULL},
#endif
				    { { T_POINTER } }
				};
    struct vector *difference;
    struct svalue *source, *dest;
    int i;

    vtmp.item[0].u.vec = subtrahend;
    vtmpp = order_alist(&vtmp);
    subtrahend = vtmpp->item[0].u.vec;
    difference = allocate_array(minuend->size);
    for (source = minuend->item, dest = difference->item, i = minuend->size;
      i--; source++) {
        extern struct svalue const0;

        int assoc PROT((struct svalue *key, struct vector *));
	if (source->type == T_OBJECT && source->u.ob->flags & O_DESTRUCTED)
	    assign_svalue(source, &const0);
	if ( assoc(source, subtrahend) < 0 )
	    assign_svalue_no_free(dest++, source);
    }
    free_vector(vtmpp);
    return shrink_array(difference, dest - difference->item);
}

/* Returns an array of all objects contained in 'ob'
 */
struct vector *all_inventory(ob, override)
    struct object *ob;
	int override;
{
    struct vector *d;
    struct object *cur;
    int cnt,res;
    int display_hidden;

    if (override) {
	    display_hidden = 1;
    } else {
	    display_hidden = -1;
    }
    cnt=0;
    for (cur=ob->contains; cur; cur = cur->next_inv)
    {
       if (cur->flags & O_HIDDEN)
       {
           if (display_hidden == -1)
           {
               display_hidden = valid_hide(current_object);
           }
           if (display_hidden)
               cnt++;
       } else cnt++;
    }
    
    if (!cnt)
	return null_array();

    d = allocate_array(cnt);
    cur=ob->contains;
    
    for (res=0;res<cnt;res++) {
        if ((cur->flags & O_HIDDEN) && !display_hidden)
        {
            cur = cur->next_inv;
            res--;
            continue;
        }
	d->item[res].type=T_OBJECT;
	d->item[res].u.ob = cur;
	add_ref(cur,"all_inventory");
	cur=cur->next_inv;
    }
    return d;
}


/* Runs all elements of an array through ob::func
   and replaces each value in arr by the value returned by ob::func
   */
struct vector *map_array (arr, func, ob, extra)
    struct vector *arr;
    char *func;
    struct object *ob;
    struct svalue *extra;
{
    extern struct svalue *sp;
    struct vector *r;
    struct svalue *v;
    int cnt;
    
    if (arr->size < 1) 
	return null_array();

    r = allocate_array(arr->size);
    
    push_vector( r );

    for (cnt = 0; cnt < arr->size; cnt++)
    {
	if (ob->flags & O_DESTRUCTED)
	    error("object used by map_array destructed\n"); /* amylaar */
	push_svalue(&arr->item[cnt]);

	if (extra)
	{
	    push_svalue (extra);
	    v = apply(func, ob, 2);
	}
	else 
	{
	    v = apply(func,ob,1);
	}
	if (v)
	    assign_svalue_no_free (&r->item[cnt], v);
    }

    r->ref--;
    sp--;

    return r;
}

static struct object *sort_array_cmp_ob;
static char *sort_array_cmp_func;
INLINE static int sort_array_cmp();
INLINE static int builtin_sort_array_cmp_fwd();
INLINE static int builtin_sort_array_cmp_rev();

#define COMPARE_NUMS(x,y) (x < y ? -1 : (x > y ? 1 : 0))

struct vector *builtin_sort_array(inlist, dir)
    struct vector *inlist;
    int dir;
{
    quickSort((char *)inlist->item, inlist->size, sizeof(inlist->item),
		dir ? builtin_sort_array_cmp_rev : builtin_sort_array_cmp_fwd);
 
    return inlist;
}

INLINE static int builtin_sort_array_cmp_fwd(p1, p2)
struct svalue *p1, *p2;
{
    if (p1->type == T_STRING && p2->type == T_STRING)
        return strcmp(p1->u.string, p2->u.string);

    if (p1->type == T_NUMBER && p2->type == T_NUMBER)
        return COMPARE_NUMS(p1->u.number, p2->u.number);

    if (p1->type == T_REAL && p2->type == T_REAL)
        return COMPARE_NUMS(p1->u.real, p2->u.real);

/* arrays are compared by comparing the first element.  Allows databases
   to be sorted. */
    if (p1->type == T_POINTER && p2->type == T_POINTER) {
      int t1, t2;
      if (p1->u.vec->size == 0 || p1->u.vec->size == 0)
	error("Illegal to have empty array in array for sort_array()\n");
      t1 = p1->u.vec->item[0].type;
      t2 = p2->u.vec->item[0].type;

      if (t1 == T_STRING && t2 == T_STRING)
        return strcmp(p1->u.vec->item[0].u.string, p2->u.vec->item[0].u.string);
      if (t1 == T_NUMBER && t2 == T_NUMBER)
        return COMPARE_NUMS(p1->u.vec->item[0].u.number, p2->u.vec->item[0].u.number);
      if (t1 == T_REAL && t2 == T_REAL)
        return COMPARE_NUMS(p1->u.vec->item[0].u.real, p2->u.vec->item[0].u.real);
    }

    error("built-in sort_array() can only handle homogeneous arrays of strings/ints/floats\n");
    return 0; /* not reached */
}

INLINE static int builtin_sort_array_cmp_rev(p1, p2)
struct svalue *p1, *p2;
{
    if (p1->type == T_STRING && p2->type == T_STRING)
        return strcmp(p2->u.string, p1->u.string);

    if (p1->type == T_NUMBER && p2->type == T_NUMBER)
        return COMPARE_NUMS(p2->u.number, p1->u.number);

    if (p1->type == T_REAL && p2->type == T_REAL)
        return COMPARE_NUMS( p2->u.real, p1->u.real );

/* arrays are compared by comparing the first element.  Allows databases
   to be sorted. */
    if (p1->type == T_POINTER && p2->type == T_POINTER) {
      int t1, t2;
      if (p1->u.vec->size == 0 || p1->u.vec->size == 0)
	error("Illegal to have empty array in array for sort_array()\n");
      t1 = p1->u.vec->item[0].type;
      t2 = p2->u.vec->item[0].type;

      if (t1 == T_STRING && t2 == T_STRING)
        return strcmp(p2->u.vec->item[0].u.string, p1->u.vec->item[0].u.string);
      if (t1 == T_NUMBER && t2 == T_NUMBER)
        return COMPARE_NUMS(p2->u.vec->item[0].u.number, p1->u.vec->item[0].u.number);
      if (t1 == T_REAL && t2 == T_REAL)
        return COMPARE_NUMS(p2->u.vec->item[0].u.real, p1->u.vec->item[0].u.real);
    }
    error("built-in sort_array() can only handle homogeneous arrays of strings/ints/floats\n");
    return 0; /* not reached */
}

struct vector *sort_array(inlist, func, ob)
    struct vector *inlist;
    char *func;
    struct object *ob;
{
    /*
     * Make func and ob globally available for sort_array_cmp used by qsort
     */
    sort_array_cmp_func = func;
    sort_array_cmp_ob = ob;
 
    /*
     * Do the sort.  Use quickSort (in qsort.c) rather than qsort()
     * because qsort() isn't tolerant of bad compare functions.
     */
    quickSort((char *)inlist->item, inlist->size, sizeof(inlist->item),
		sort_array_cmp);
 
    return inlist;
}
 
 
INLINE static int sort_array_cmp(p1, p2)
struct svalue *p1, *p2;
{
	int ret;
	struct svalue *d;
 
	if (sort_array_cmp_ob->flags & O_DESTRUCTED)
	  error("object used by sort_array destructed\n");
	push_svalue(p1);
	push_svalue(p2);
	d = apply(sort_array_cmp_func, sort_array_cmp_ob, 2);
	if (!d) {
	  return 0;
	}
	if (d->type != T_NUMBER) {    
	  ret = 0;
	} else {
	  ret = d->u.number;
	}
	return ret;
}

/*
 * deep_inventory()
 *
 * This function returns the recursive inventory of an object. The returned 
 * array of objects is flat, ie there is no structure reflecting the 
 * internal containment relations.
 *
 * This is Robocoder's deep_inventory().  It uses two passes in order to
 * avoid costly temporary arrays (allocating, copying, adding, freeing, etc).
 * The recursive call routines are:
 *    deep_inventory_count() and deep_inventory_collect()
 */
static int valid_hide_flag;

static int deep_inventory_count(ob)
    struct object *ob;
{
    struct object *cur;
    int cnt;

    cnt = 0;

    /* step through object's inventory and count visible objects */
    for (cur = ob->contains; cur; cur = cur->next_inv) {
        if (cur->flags & O_HIDDEN) {
            if (!valid_hide_flag)
                valid_hide_flag = 1 +
                      (valid_hide(current_object) ? 1 : 0);
            if (valid_hide_flag & 2) {
                cnt++;
                cnt += deep_inventory_count(cur);
            }
        } else {
            cnt++;
            cnt += deep_inventory_count(cur);
        }
    }
    
    return cnt;
}     

static void deep_inventory_collect(ob, inv, i)
    struct object *ob;
    struct vector *inv;
    int *i;
{
    struct object *cur;

    /* step through object's inventory and look for visible objects */
    for (cur = ob->contains; cur; cur = cur->next_inv) {
        if (cur->flags & O_HIDDEN) {
            if (valid_hide_flag & 2) {
                inv->item[*i].type = T_OBJECT;
                inv->item[*i].u.ob = cur;
                (*i)++;
                add_ref(cur, "deep_inventory_collect");

                deep_inventory_collect(cur, inv, i);
            }
        } else {
            inv->item[*i].type = T_OBJECT;
            inv->item[*i].u.ob = cur;
            (*i)++;
            add_ref(cur, "deep_inventory_collect");

            deep_inventory_collect(cur, inv, i);
        }
    }
}     

struct vector *deep_inventory(ob, take_top)
    struct object	*ob;
{
    struct vector *dinv;
    int i;

    valid_hide_flag = 0;

    /*
     * count visible objects in an object's inventory, and in their
     * inventory, etc
     */
    i = deep_inventory_count(ob);
    if (take_top)
        i++;

    if (i == 0)
	return null_array();

    /*
     * allocate an array
     */
    dinv = allocate_array(i);
    if (take_top) {
        dinv->item[0].type = T_OBJECT;
        dinv->item[0].u.ob = ob;
        add_ref(ob, "deep_inventory");
    }

    /*
     * collect visible inventory objects recursively
     */
    i = 0;
    push_vector(dinv); /* in case of error */
    deep_inventory_collect(ob, dinv, &i);
    pop_stack();

    return dinv;
}

static INLINE int alist_cmp(p1, p2)
    struct svalue *p1,*p2;
{
    register int d;

    if ((d = p1->u.number - p2->u.number)) return d;
    if ((d = p1->type - p2->type)) return d;
    return 0;
}

struct vector *order_alist(inlist)
    struct vector *inlist;
{
    struct vector *outlist;
    struct svalue *inlists, *outlists, *root, *inpnt, *insval;
    int listnum, keynum, i, j;

    listnum = inlist->size;
    inlists = inlist->item;
    keynum = inlists[0].u.vec->size;
    root = inlists[0].u.vec->item;
    /* transform inlists[i].u.vec->item[j] into a heap, starting at the top */
    insval = (struct svalue*)
		DXALLOC(sizeof(struct svalue[1])*listnum, 13, "order_alist: insval");
    for(j=0,inpnt=root; j<keynum; j++,inpnt++) {
	int curix, parix;

	/* make sure that strings can be compared by their pointer */
	if (inpnt->type == T_STRING && inpnt->subtype != STRING_SHARED) {
	    char *str = make_shared_string(inpnt->u.string);
	    free_svalue(inpnt);
	    inpnt->type = T_STRING;
	    inpnt->subtype = STRING_SHARED;
	    inpnt->u.string = str;
	}
	/* propagate the new element up in the heap as much as necessary */
	for (i=0; i<listnum; i++) {
	    insval[i] = inlists[i].u.vec->item[j];
	    /* but first ensure that it contains no destructed object */
	    if (insval[i].type == T_OBJECT
	      && insval[i].u.ob->flags & O_DESTRUCTED) {
                extern struct svalue const0;

		free_object(insval[i].u.ob, "order_alist");
	        inlists[i].u.vec->item[j] = insval[i] = const0;
	    }
	}
	for(curix = j; curix; curix=parix) {
	    parix = (curix-1)>>1;
	    if ( alist_cmp(&root[parix], &root[curix]) > 0 ) {
		for (i=0; i<listnum; i++) {
		    inlists[i].u.vec->item[curix] =
		      inlists[i].u.vec->item[parix];
		    inlists[i].u.vec->item[parix] = insval[i];
		}
	    }
	}
    }
    FREE((char*)insval);
    outlist = allocate_array(listnum);
    outlists = outlist->item;
    for (i=0; i<listnum; i++) {
	outlists[i].type  = T_POINTER;
	outlists[i].u.vec = allocate_array(keynum);
    }
    for(j=0; j<keynum; j++) {
	int curix;

	for (i=0;  i<listnum; i++) {
	    outlists[i].u.vec->item[j] = inlists[i].u.vec->item[0];
	}
	for (curix=0; ; ) {
	    int child, child2;

	    child = curix+curix+1;
	    child2 = child+1;
	    if (child2<keynum && root[child2].type != T_INVALID
	      && (root[child].type == T_INVALID ||
		alist_cmp(&root[child], &root[child2]) > 0))
	    {
		child = child2;
	    }
	    if (child<keynum && root[child].type != T_INVALID) {
		for (i=0; i<listnum; i++) {
		    inlists[i].u.vec->item[curix] =
		      inlists[i].u.vec->item[child];
		}
		curix = child;
	    } else break;
	}
	for (i=0; i<listnum; i++) {
	    inlists[i].u.vec->item[curix].type = T_INVALID;
	}
    }
    return outlist;
}

static int search_alist(key, keylist)
    struct svalue *key;
    struct vector *keylist;
{
    int i, o, d;

    if (!keylist->size) return 0;
    i = (keylist->size) >> 1;
    o = (i+2) >> 1;
    for (;;) {
	d = alist_cmp(key, &keylist->item[i]);
	if (d<0) {
	    i -= o;
	    if (i<0) {
		i = 0;
	    }
	} else if (d>0) {
	    i += o;
	    if (i >= keylist->size) {
	        i = keylist->size-1;
	    }
	} else {
	    return i;
	}
	if (o<=1) {
	    if (alist_cmp(key, &keylist->item[i]) > 0) return i+1;
	    return i;
	}
	o = (o+1) >> 1;
    }
}

int assoc(key, list)
    struct svalue *key;
    struct vector *list;
{
    int i;
    extern char* findstring PROT((char*));

    if (key->type == T_STRING && key->subtype != STRING_SHARED) {
        static struct svalue stmp = { T_STRING, STRING_SHARED };

        stmp.u.string = findstring(key->u.string);
        if (!stmp.u.string) return -1;
        key = &stmp;
    }
    i = search_alist(key, list);
    if (i == list->size || alist_cmp(key, &list->item[i]))
        i = -1;
    return i;
}

struct vector *intersect_alist(a1, a2)
    struct vector *a1,*a2;
{
    struct vector *a3;
    int d, l, i1, i2, a1s, a2s;

    a1s = a1->size;
    a2s = a2->size;
    a3 = allocate_array( a1s < a2s ? a1s : a2s);
    for (i1=i2=l=0; i1 < a1s && i2 < a2s; ) {
	d = alist_cmp(&a1->item[i1], &a2->item[i2]);
	if (d<0)
	    i1++;
	else if (d>0)
	    i2++;
	else {
	    assign_svalue_no_free(&a3->item[l++], &a2->item[(i1++,i2++)]);
	}
    }
    return shrink_array(a3, l);
}

struct vector *intersect_array(a1, a2)
    struct vector *a1,*a2;
{
    struct vector *vtmpp1,*vtmpp2,*vtmpp3;
    static struct vector vtmp = { 1, 1,
#ifdef DEBUG
				    1,
#endif
#ifndef NO_MUDLIB_STATS
				    {(mudlib_stats_t *)NULL,
				       (mudlib_stats_t *)NULL},
#endif
				    { { T_POINTER } }
				};

    if (a1->ref > 1) {
        vtmpp1 = a1;
	a1 = slice_array(a1, 0, a1->size-1);
	free_vector(vtmpp1);
    }
    vtmp.item[0].u.vec = a1;
    vtmpp1 = order_alist(&vtmp);
    free_vector(vtmp.item[0].u.vec);
    if (a2->ref > 1) {
        vtmpp2 = a2;
	a2 = slice_array(a2, 0, a2->size-1);
	free_vector(vtmpp2);
    }
    vtmp.item[0].u.vec = a2;
    vtmpp2 = order_alist(&vtmp);
    free_vector(vtmp.item[0].u.vec);
    vtmpp3 = intersect_alist(vtmpp1->item[0].u.vec, vtmpp2->item[0].u.vec);
    free_vector(vtmpp1);
    free_vector(vtmpp2);
    return vtmpp3;
}

struct vector *match_regexp(v, pattern)
    struct vector *v;
    char *pattern;
{
    struct regexp *reg;
    char *res;
    int i, num_match;
    struct vector *ret;
    extern int eval_cost;

    if (v->size == 0)
	return null_array();
    reg = regcomp(pattern, 0);
    if (reg == 0)
	return 0;
    res = (char *)DMALLOC(v->size, 14, "match_regexp: res");
    for (num_match=i=0; i < v->size; i++) {
	res[i] = 0;
	if (v->item[i].type != T_STRING)
	    continue;

    if ((--eval_cost) <= 0)  return 0;

	if (regexec(reg, v->item[i].u.string) == 0)
	    continue;
	res[i] = 1;
	num_match++;
    }
    ret = allocate_array(num_match);
    for (num_match=i=0; i < v->size; i++) {
	if (res[i] == 0)
	    continue;
	assign_svalue_no_free(&ret->item[num_match], &v->item[i]);
	num_match++;
    }
	FREE(res);
    FREE((char *)reg);
    return ret;
}

/*
 * Returns a list of all inherited files.
 *
 * Must be fixed so that any number of files can be returned, now max 256
 * (Sounds like a contradiction to me /Lars).
 */
struct vector *deep_inherit_list(ob)
    struct object *ob;
{
    struct vector *ret;
    struct program *pr, *plist[256];
    int il, il2, next, cur;

    plist[0] = ob->prog; next = 1; cur = 0;
    
    for (; cur < next && next < 256; cur++)
    {
	pr = plist[cur];
	for (il2 = 0; il2 < (int)pr->p.i.num_inherited; il2++)
	    plist[next++] = pr->p.i.inherit[il2].prog;
    }
	    
	next--;
    ret = allocate_array(next);

    for (il = 0; il < next; il++)
    {
	pr = plist[il + 1];
	ret->item[il].type = T_STRING;
	ret->item[il].subtype = STRING_SHARED;
	ret->item[il].u.string = 
		make_shared_string(pr->name);
    }
    return ret;
}

/*
 * Returns a list of the immediate inherited files.
 *
 */
struct vector *inherit_list(ob)
struct object *ob;
{
	struct vector *ret;
	struct program *pr, *plist[256];
	int il, il2, next, cur;

	plist[0] = ob->prog; next = 1; cur = 0;
    
	pr = plist[cur];
	for (il2 = 0; il2 < (int)pr->p.i.num_inherited; il2++) {
		plist[next++] = pr->p.i.inherit[il2].prog;
	}
 
	next--; /* don't count the file itself */
	ret = allocate_array(next);

	for (il = 0; il < next; il++) {
		pr = plist[il + 1];
		ret->item[il].type = T_STRING;
		ret->item[il].subtype = STRING_SHARED;
		ret->item[il].u.string = make_shared_string(pr->name);
	}
	return ret;
}

struct vector *
children(str)
char *str;
{
	extern struct object *obj_list;
	int i,j;
	int t_sz;
	int sl, ol, needs_freed = 0;
	struct object *ob;
	struct object **tmp_children;
	struct vector *ret;
        int display_hidden;

        display_hidden = -1;
	/* Skip over leading '/' if any. */
	while(str[0] == '/') {
		str++;
	}
	/* Truncate possible .c in the object name. */
	sl = strlen(str);
	if ((sl > 2) && (str[sl-2] == '.') &&
#ifndef LPC_TO_C
          (str[sl-1] == 'c')) {
#else
          ((str[sl-1] == 'c') || (str[sl-1]=='C')))  {
#endif
		char *p;
		/*
		  A new writable copy of the name is needed.
		*/
		/* (sl - 1) == minus ".c" plus "\0" */
		p = (char *)DMALLOC(sl - 1, 15, "children: p"); 
		strncpy(p, str, sl - 2);
		p[sl - 2] = '\0';
		sl -= 2; /* removed the ".c" */
		str = p;
		needs_freed = 1;
    }
	if (!(tmp_children = (struct object **)
		DMALLOC(sizeof(struct object *) * (t_sz = 50),
			16, "children: tmp_children")))
	{
		if (needs_freed) FREE(str);
		return null_array(); /* unable to malloc enough temp space */
	}
	for (i = 0, ob = obj_list; ob; ob = ob->next_all) {
		ol = strlen(ob->name);
		if (((ol == sl) || ((ol > sl) && (ob->name[sl] == '#')))
			&& !strncmp(str,ob->name,sl))
		{
                if (ob->flags & O_HIDDEN)
                {
                    if (display_hidden == -1)
                    {
                  display_hidden = valid_hide(current_object);
                    }
                    if (!display_hidden) continue;
                }
			tmp_children[i] = ob;
			if ((++i == t_sz) &&(!(tmp_children
				= (struct object **)DREALLOC((void *)tmp_children,
					sizeof(struct object *) * (t_sz += 50),
					17, "children: tmp_children: realloc"))))
			{   /* unable to REALLOC more space (tmp_children is now NULL) */
				if (needs_freed) FREE(str);
				return null_array();
			}
		}
	}
	if(i > max_array_size) {
		i = max_array_size;
	}
	ret = allocate_array(i);
	for (j = 0; j < i; j++) {
		ret->item[j].type = T_OBJECT;
		ret->item[j].u.ob = tmp_children[j];
		add_ref(tmp_children[j],"children");
	}
	if (needs_freed) {
		FREE(str);
	}
	FREE((void *)tmp_children);
	return ret;
}

struct vector *
livings()
{
    int nob, apply_valid_hide, hide_is_valid = 0;
    struct object *ob, **obtab;
    struct vector *vec;

    nob = 0;
    apply_valid_hide = 1;

    obtab = (struct object **)
		DMALLOC(max_array_size * sizeof(struct object **), 18, "livings");

    for (ob = obj_list; ob != NULL; ob = ob->next_all) {
	if ((ob->flags & O_ENABLE_COMMANDS) == 0)
	    continue;
	if (ob->flags & O_HIDDEN) {
	    if (apply_valid_hide) {
		apply_valid_hide = 0;
		hide_is_valid = valid_hide(current_object);
	    }
	    if (hide_is_valid)
		continue;
	}
	if (nob == max_array_size)
	    break;
	obtab[nob++] = ob;
    }

    vec = allocate_array(nob);
    while (--nob >= 0) {
	vec->item[nob].type = T_OBJECT;
	vec->item[nob].u.ob = obtab[nob];
	add_ref(obtab[nob], "livings");
    }

    FREE(obtab);

    return vec;
}

struct vector *
objects(select_func, select_ob)
  char *select_func;
  struct object *select_ob;
{
  extern struct object *obj_list;
  extern struct svalue *sp;
  int i, j, t_sz;
  struct object *ob;
  struct object **tmp;
  struct vector *ret;
  int display_hidden;
  struct svalue *v;
  int zerop;
 
  display_hidden = -1;
 
  if (!(tmp = (struct object **)DMALLOC(sizeof(struct object *) * (t_sz = 1000),
        16, "objects: tmp")))
    fatal("Out of memory!\n");

  /* disguise DMALLOC()'d vector */
  push_malloced_string((char *)tmp);
 
  for (i = 0, ob = obj_list; ob; ob = ob->next_all) {
    if (ob->flags & O_HIDDEN) {
      if (display_hidden == -1)
        display_hidden = valid_hide(current_object);
      if (!display_hidden) continue;
    }
    if (select_func != NULL) {
      push_object(ob);
#if 0 /* safe_apply doesn't seem to be up to the task */
      v = safe_apply(select_func, select_ob, 1);
#else /* just take the memory hit for now so as not to crash the driver */
      v = apply(select_func, select_ob, 1);
#endif
      zerop = IS_ZERO(v);
      if (!v) {
         FREE((void *)tmp);
         sp--;
         return 0;
      }
      if (zerop)
        continue;
    }
    tmp[i] = ob;
    if (++i == t_sz) {
      if (!(tmp = (struct object **)DREALLOC((void *)tmp,
            sizeof(struct object *) * (t_sz += 1000),
            17, "objects: tmp: realloc")))
        fatal("Out of memory!\n");
      else
        sp->u.string = (char *)tmp;
    }
  }
  if (i > max_array_size)
    i = max_array_size;
  ret = allocate_array(i);
  for (j = 0; j < i; j++) {
    ret->item[j].type = T_OBJECT;
    ret->item[j].u.ob = tmp[j];
    add_ref(tmp[j], "objects");
  }

  FREE((void *)tmp);
  sp--;

  return ret;
}
