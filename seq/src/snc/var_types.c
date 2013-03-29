/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "main.h"
#include "expr.h"

/* #define DEBUG */

const char *type_name(unsigned tag)
{
    switch (tag) {
        case V_NONE:    return "foreign";
        case V_EVFLAG:  return "evflag";
        case V_CHAR:    return "char";
        case V_UCHAR:   return "unsigned char";
        case V_SHORT:   return "short";
        case V_USHORT:  return "unsigned short";
        case V_INT:     return "int";
        case V_UINT:    return "unsigned int";
        case V_LONG:    return "long";
        case V_ULONG:   return "unsigned long";
        case V_INT8T:   return "epicsInt8";
        case V_UINT8T:  return "epicsUInt8";
        case V_INT16T:  return "epicsInt16";
        case V_UINT16T: return "epicsUInt16";
        case V_INT32T:  return "epicsInt32";
        case V_UINT32T: return "epicsUInt32";
        case V_FLOAT:   return "float";
        case V_DOUBLE:  return "double";
        case V_STRING:  return "string";
        case V_ENUM:    return "enumeration";
        default:        return "";
    }
}

Expr *decl_add_base_type(Expr *ds, unsigned tag)
{
    Expr *d;
    static const int impossible = FALSE;

    foreach(d, ds) {
        Var *var;
        Type *t = new(Type), *bt = t;

        assert(d->type == D_DECL);      /* pre-condition */
        var = d->extra.e_decl;
        assert(var);
        t->tag = tag;
        t->parent = var->type;
        /* now roll back the stack of type expressions */
        while(t->parent) {
            switch (t->parent->tag) {
            case V_ARRAY:
                if (tag == V_NONE) {
                    error_at_expr(d, "cannot declare array of foreign variables\n");
                }
                if (tag == V_EVFLAG) {
                    error_at_expr(d, "cannot declare array of event flags\n");
                }
                t->parent->val.array.elem_type = t;
                break;
            case V_POINTER:
                if (tag == V_NONE) {
                    error_at_expr(d, "cannot declare pointer to foreign variable\n");
                }
                if (tag == V_EVFLAG) {
                    error_at_expr(d, "cannot declare pointer to event flag\n");
                }
                t->parent->val.pointer.value_type = t;
                break;
            default: assert(impossible);
            }
            t = t->parent;
        }
        assert(!t->parent);
        t->parent = bt;
        var->type = t;
        if (tag == V_EVFLAG)
            var->chan.evflag = new(EvFlag);
    }
    return ds;
}

Expr *decl_add_init(Expr *d, Expr *init)
{
    assert(d->type == D_DECL);          /* pre-condition */
#ifdef DEBUG
    report("decl_add_init: var=%s, init=%p\n", d->extra.e_decl->name, init);
#endif
    d->extra.e_decl->init = init;
    d->decl_init = init;
    return d;
}

Expr *decl_create(Token name)
{
    Expr *d = expr(D_DECL, name, 0);
    Var *var = new(Var);

#ifdef DEBUG
    report("decl_create: name(%s)\n", name.str);
#endif
    assert(d->type == D_DECL);          /* expr() post-condition */
    var->name = name.str;
    d->extra.e_decl = var;
    var->decl = d;
    return d;
}

Expr *decl_postfix_array(Expr *d, char *s)
{
    Type *t = new(Type);
    uint num_elems;

    assert(d->type == D_DECL);          /* pre-condition */
    if (!strtoui(s, UINT_MAX, &num_elems) || num_elems == 0) {
        error_at_expr(d, "invalid array size (must be >= 1)\n");
        num_elems = 1;
    }

#ifdef DEBUG
    report("decl_postfix_array %u\n", num_elems);
#endif

    t->tag = V_ARRAY;
    t->val.array.num_elems = num_elems;
    t->parent = d->extra.e_decl->type;
    d->extra.e_decl->type = t;
    return d;
}

Expr *decl_prefix_pointer(Expr *d)
{
    Type *t = new(Type);

#ifdef DEBUG
    report("decl_prefix_pointer\n");
#endif
    assert(d->type == D_DECL);          /* pre-condition */
    t->tag = V_POINTER;
    t->parent = d->extra.e_decl->type;
    d->extra.e_decl->type = t;
    return d;
}

unsigned type_array_length1(Type *t)
{
    switch (t->tag) {
    case V_ARRAY:
        return t->val.array.num_elems;
    default:
        return 1;
    }
}

unsigned type_array_length2(Type *t)
{
    switch (t->tag) {
    case V_ARRAY:
        return type_array_length1(t->val.array.elem_type);
    default:
        return 1;
    }
}

static unsigned type_assignable_array(Type *t, int depth)
{
    if (depth > 2)
        return FALSE;
    switch (t->tag) {
    case V_NONE:
    case V_EVFLAG:
    case V_POINTER:
        return FALSE;
    case V_ARRAY:
        return type_assignable_array(t->val.array.elem_type, depth + 1);
    default:
        return TRUE;
    }
}

unsigned type_assignable(Type *t)
{
    return type_assignable_array(t, 0);
}

static void gen_array_pointer(Type *t, unsigned last_tag, char *name)
{
    int paren = last_tag == V_ARRAY;
    switch (t->tag) {
    case V_POINTER:
        if (paren)
            printf("(");
        printf("*");
        gen_array_pointer(t->parent, t->tag, name);
        if (paren)
            printf(")");
        break;
    case V_ARRAY:
        gen_array_pointer(t->parent, t->tag, name);
        printf("[%d]", t->val.array.num_elems);
        break;
    default:
        printf("%s", name);
    }
}

void gen_type(Type *t, char *name)
{
    Type *bt = base_type(t);

    printf("%s ", type_name(bt->tag));
    gen_array_pointer(bt->parent, V_NONE, name);
}
