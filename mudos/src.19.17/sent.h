struct sentence {
    char *verb;
    struct object *ob;
    char *function;
    struct sentence *next;
#ifndef AURORA
    int flags;
#else
    int star, priority;
#endif
};

#define V_SHORT     1
#define V_NOSPACE	2

struct sentence *alloc_sentence();

void remove_sent PROT((struct object *, struct object *)),
    free_sentence PROT((struct sentence *));
