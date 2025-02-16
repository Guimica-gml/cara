#include "./types.h"
#include <assert.h>
#include <stdio.h>

int Type_cmp(const struct Type *a, const struct Type *b) {
    int diff = a->tag - b->tag;
    if (diff) return diff;

    switch (a->tag) {
    case TT_Func:
        diff = Type_cmp(a->func.args, b->func.args);
        if (diff) return diff;
        return Type_cmp(a->func.ret, b->func.ret);
    case TT_Call:
        diff = Type_cmp(a->call.name, b->call.name);
        if (diff) return diff;
        return Type_cmp(a->call.args, b->call.args);
    case TT_Tuple: {
        const struct TypeTuple* ahead = a->tuple;
        const struct TypeTuple* bhead = b->tuple;
        while (ahead && bhead) {
            diff = Type_cmp(ahead->current, bhead->current);
            if (diff) return diff;
            ahead = ahead->next;
            bhead = bhead->next;
        }
        return ahead - bhead;
    }
    case TT_Recall:
        diff = a->recall.len - b->recall.len;
        if (diff) return diff;
        return a->recall.str - b->recall.str;
    case TT_Var:
        return a->var - b->var;
    }

    assert(0 && "no gcc, control doesn't reach here");
}

void Type_print(const struct Type *this) {
    switch (this->tag) {
    case TT_Func:
        printf("func(");
        Type_print(this->func.args);
        printf("):");
        Type_print(this->func.ret);
        return;
    case TT_Call:
        Type_print(this->call.name);
        printf("(");
        Type_print(this->call.args);
        printf(")");
        return;
    case TT_Tuple: {
        printf("(");
        const struct TypeTuple *head;
        for (head = this->tuple; head && head->next; head = head->next) {
            Type_print(head->current);
            printf(", ");
        }
        if (head) {
            Type_print(head->current);
        }
        printf(")");
        return;
    }
    case TT_Recall:
        printf("%s", this->recall.str);
        return;
    case TT_Var:
        printf("var(%d)", this->var);
    }
}
