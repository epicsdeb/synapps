/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INCLvar_typesh
#define INCLvar_typesh

enum type_tag {
    V_NONE,
    V_EVFLAG,
    V_CHAR,
    V_UCHAR,
    V_SHORT,
    V_USHORT,
    V_INT,
    V_UINT,
    V_LONG,
    V_ULONG,
    V_INT8T,
    V_UINT8T,
    V_INT16T,
    V_UINT16T,
    V_INT32T,
    V_UINT32T,
    V_FLOAT,
    V_DOUBLE,
    V_STRING,
    V_ENUM,
    V_POINTER,
    V_ARRAY,
};

struct array_type {
    unsigned    num_elems;
    struct type *elem_type;
};

struct pointer_type {
    struct type *value_type;
};

struct enum_type {
    unsigned    num_names;
    char        **names;
};

typedef struct type Type;

struct type {
    enum type_tag tag;
    union {
        struct pointer_type pointer;
        struct array_type   array;
        struct enum_type    enumeration;
    } val;
    struct type *parent;
};

const char *type_name (unsigned tag);
#define type_base_type(t) (t->parent->tag)
#define base_type(t) (t->parent)
unsigned type_array_length1(Type *t);
unsigned type_array_length2(Type *t);
unsigned type_assignable(Type *t);
void gen_type(Type *t, char *name);

#endif /*INCLvar_typesh */
