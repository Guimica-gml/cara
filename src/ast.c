#include "./ast.h"
#include "common_ll.h"
#include <assert.h>
#include <stdio.h>

const struct Type *Binding_to_type(struct TypeIntern *intern, struct Binding this) {
    switch (this.tag) {
    case BT_Empty:
        return intern->tsyms.t_unit;
    case BT_Name:
        return this.name.annot;
    case BT_Tuple: {
        const struct Type *out = intern->tsyms.t_unit;
        for (ll_iter(head, this.tuple)) {
            const struct Type* current = Binding_to_type(intern, head->current);
            out = Type_tuple_extend(intern, out, current);
        }
        return Type_call(intern, intern->tsyms.t_star, out);
    }
    }
    assert(false && "gcc complains about control reaching here??");
}

void TypeIntern_print(struct TypeIntern *this) { Typereg_print(&this->tree); }

struct TypeIntern TypeIntern_init(struct serene_Trea* alloc, struct Symbols syms) {
    struct Type t_unit = {.tag = TT_Tuple, .tuple = NULL};
    struct Type t_bool = {.tag = TT_Recall, .recall = syms.s_bool};
    struct Type t_int = {.tag = TT_Recall, .recall = syms.s_int};
    struct Type t_string = {.tag = TT_Recall, .recall = syms.s_string};
    struct Type t_star = {.tag = TT_Recall, .recall = syms.s_star};

    struct TypeIntern out = {0};
    out.alloc = alloc;
    out.syms = syms;
    out.tsyms = (struct Typesyms){
#define ins(t) .t = TypeIntern_intern(&out, &t)
        ins(t_unit), ins(t_bool), ins(t_int), ins(t_string), ins(t_star),
#undef ins
    };
    return out;
}

const struct Type *TypeIntern_intern(struct TypeIntern *this, struct Type *t) {
    printf("interning: ");
    Type_print(t);
    printf("\n");
    struct Type **entry = Typereg_search(&this->tree, t);
    if (entry) {
        printf("found (%p)!\n\n", entry);
        return *entry;
    }
    printf("found nothing!\ninserting!\n");

    struct Type *new = serene_trealloc(this->alloc, struct Type);
    assert(new && "OOM");
    *new = *t;
    assert(Typereg_insert(&this->tree, serene_Trea_dyn(this->alloc), new));
    printf("inserted (%p)\n\n", new);
    return new;
}

const struct Type *Type_recall(struct TypeIntern *intern, struct String name) {
    struct Type recall = {.tag = TT_Recall, .recall = name};
    return TypeIntern_intern(intern, &recall);
}

const struct Type *Type_func(
    struct TypeIntern *intern, const struct Type *args, const struct Type *ret
) {
    struct Type func = {.tag = TT_Func, .func.args = args, .func.ret = ret};
    return TypeIntern_intern(intern, &func);
}

const struct Type* Type_tuple(
    struct TypeIntern* intern,
    const struct Type* lhs,
    const struct Type* rhs
) {
    struct TypeTuple* node_lhs = serene_trealloc(intern->alloc, struct TypeTuple);
    struct TypeTuple* node_rhs = serene_trealloc(intern->alloc, struct TypeTuple);
    assert(node_lhs && node_rhs && "OOM");
    *node_lhs = (struct TypeTuple){0};
    *node_rhs = (struct TypeTuple){0};
    node_lhs->current = lhs;
    node_rhs->current = rhs;
    node_rhs->next = node_lhs;
    return TypeIntern_intern(intern, &(struct Type){.tag = TT_Tuple, .tuple = node_rhs});
}

const struct Type* Type_tuple_extend(
    struct TypeIntern* intern,
    const struct Type* tail,
    const struct Type* head
) {
    if (tail->tag == TT_Tuple) {
        const struct TypeTuple* bare_tail = tail->tuple;
        struct TypeTuple* new = serene_trealloc(intern->alloc, struct TypeTuple);
        assert(new && "OOM");
        *new = (struct TypeTuple){0};
        new->current = head;
        new->next = bare_tail;
        return TypeIntern_intern(intern, &(struct Type){.tag = TT_Tuple, .tuple = new});
    } else {
        return Type_tuple(intern, tail, head);
    }
}

const struct Type *Type_call(
    struct TypeIntern *intern, const struct Type *name, const struct Type *args
) {
    struct Type call = {.tag = TT_Call, .call.name = name, .call.args = args};
    return TypeIntern_intern(intern, &call);
}

const struct Type* Type_new_typevar(struct TypeIntern* intern) {
    struct Type var = {.tag = TT_Var, .var = intern->var_counter++};
    return TypeIntern_intern(intern, &var);
}

static void Binding_print(struct Binding *binding) {
    switch (binding->tag) {
    case BT_Empty:
        printf("()");
        break;
    case BT_Name:
        printf("%s: ", binding->name.name.str);
        Type_print(binding->name.annot);
        break;
    case BT_Tuple:
        printf("(");
        struct BindingTuple *head;
        for (head = binding->tuple; head && head->next; head = head->next) {
            Binding_print(&head->current);
            printf(", ");
        }
        if (head) {
            Binding_print(&head->current);
        }
        printf(")");
    }
}

static void print_ET_If(struct ExprIf* expr, int level),
    print_ET_Loop(struct Expr* body, int level),
    print_ET_Bareblock(struct ExprsLL* body, int level),
    print_ET_Call(struct ExprCall* expr, int level),
    print_ET_Tuple(struct ExprTuple expr, int level),
    print_ST_Let(struct ExprLet *expr, int level),
    print_ST_Mut(struct ExprLet *expr, int level),
    print_ST_Break(struct Expr *expr, int level),
    print_ST_Return(struct Expr *expr, int level),
    print_ST_Assign(struct ExprAssign *expr, int level),
    print_ST_Const(struct Expr *expr, int level);

static void Expr_print(struct Expr *expr, int level) {
#define Case(Tag, ...) \
    case Tag: print_##Tag(__VA_ARGS__); break;

    switch (expr->tag) {
        Case(ET_If, expr->if_expr, level);
        Case(ET_Loop, expr->loop, level);
        Case(ET_Bareblock, expr->bareblock, level);
        Case(ET_Call, expr->call, level);
        Case(ET_Tuple, expr->tuple, level);

        Case(ST_Let, expr->let, level);
        Case(ST_Mut, expr->let, level);
        Case(ST_Break, expr->break_stmt, level);
        Case(ST_Return, expr->return_stmt, level);
        Case(ST_Assign, expr->assign, level);
        Case(ST_Const, expr->const_stmt, level);

    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
    case ET_Recall:
        printf("%s", expr->lit.str);
        break;
    }

#undef Case
}

static void print_ET_If(struct ExprIf *expr, int level) {
    printf("if ");
    Expr_print(&expr->cond, level);
    printf(" ");
    Expr_print(&expr->smash, level);
    printf(" else ");
    Expr_print(&expr->pass, level);
}

static void print_ET_Loop(struct Expr *body, int level) {
    printf("loop ");
    Expr_print(body, level);
}

static void print_ET_Bareblock(struct ExprsLL *body, int level) {
    printf("{\n");
    for (ll_iter(h, body)) {
        for (int i = 0; i < level + 1; i++)
            printf("  ");
        Expr_print(&h->current, level + 1);
        printf("\n");
    }
    for (int i = 0; i < level; i++)
        printf("  ");
    printf("}");
}

static void print_ET_Call(struct ExprCall *expr, int level) {
    Expr_print(&expr->name, level);
    printf("(");
    Expr_print(&expr->args, level + 1);
    printf(")");
}

static void print_ET_Tuple(struct ExprTuple expr, int level) {
    printf("(");
    struct ExprsLL *head;
    for (head = expr.list; head && head->next; head = head->next) {
        Expr_print(&head->current, level);
        printf(", ");
    }
    if (head) {
        Expr_print(&head->current, level);
    }
    printf(")");
}

static void print_ST_Let(struct ExprLet *expr, int level) {
    printf("let ");
    Binding_print(&expr->bind);
    printf(" = ");
    Expr_print(&expr->init, level);
}

static void print_ST_Mut(struct ExprLet *expr, int level) {
    printf("mut ");
    Binding_print(&expr->bind);
    printf(" = ");
    Expr_print(&expr->init, level);
}

static void print_ST_Break(struct Expr *expr, int level) {
    printf("break ");
    Expr_print(expr, level);
}

static void print_ST_Return(struct Expr *expr, int level) {
    printf("return ");
    Expr_print(expr, level);
}

static void print_ST_Assign(struct ExprAssign *expr, int level) {
    printf("%s = ", expr->name.str);
    Expr_print(&expr->expr, level);
}

static void print_ST_Const(struct Expr *expr, int level) {
    Expr_print(expr, level);
    printf(";");
}

static void Function_print(struct Function *func, int level) {
    for (int i = 0; i < level; i++)
        printf("  ");
    printf("func %s(", func->name.str);
    Binding_print(&func->args);
    printf("): ");
    Type_print(func->ret);
    printf(" ");
    Expr_print(&func->body, level);
}

void Ast_print(struct Ast* ast) {
    for (ll_iter(f, ast->funcs)) {
        Function_print(&f->current, 0);
        printf("\n");
    }
}

struct Expr Expr_tuple(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Expr lhs,
    struct Expr rhs
) {
    struct ExprsLL* list = serene_trealloc(alloc, struct ExprsLL);
    struct ExprsLL* last = serene_trealloc(alloc, struct ExprsLL);
    assert(list && last && "OOM");
    *list = (typeof(*list)){0};
    *last = (typeof(*last)){0};
    list->current = lhs;
    last->current = rhs;
    list->next = last;
    const struct Type *type = Type_tuple(intern, lhs.type, rhs.type);
    return (struct Expr) {
        .tag = ET_Tuple,
        .type = Type_call(intern, intern->tsyms.t_star, type),
        .tuple = (struct ExprTuple) {
            .list = list,
            .last = last,
        },
    };
}

struct Expr Expr_tuple_extend(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Expr tail,
    struct Expr head
) {
    if (tail.tag != ET_Tuple) return Expr_tuple(alloc, intern, tail, head);
    struct ExprsLL* tmp = serene_trealloc(alloc, struct ExprsLL);
    assert(tmp && "OOM");
    *tmp = (typeof(*tmp)){0};
    tmp->current = head;
    if (!tail.tuple.last) tail.tuple.list = tmp;
    else tail.tuple.last->next = tmp;
    tail.tuple.last = tmp;
    tail.type = Type_call(
        intern,
        intern->tsyms.t_star,
        Type_tuple_extend(intern, tail.type->call.args, head.type)
    );
    return tail;
}

struct Expr Expr_unit(struct TypeIntern* intern) {
    return (struct Expr){.tag = ET_Tuple, .type = intern->tsyms.t_unit, .tuple = {0}};
}

struct Expr Expr_call(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Expr name,
    struct Expr args
) {
    const struct Type* type = Type_new_typevar(intern);
    struct ExprCall* call = serene_trealloc(alloc, struct ExprCall);
    assert(call && "OOM");
    call->name = name;
    call->args = args;
    return (struct Expr) { .tag = ET_Call, .type = type, .call = call };
}

struct Expr Expr_if(
    struct serene_Trea* alloc,
    struct Expr cond,
    struct Expr smash,
    struct Expr pass
) {
    struct ExprIf* body = serene_trealloc(alloc, struct ExprIf);
    assert(body && "OOM");
    body->cond = cond;
    body->smash = smash;
    body->pass = pass;
    const struct Type* type = smash.type;
    return (struct Expr) { .tag = ET_If, .type = type, .if_expr = body };
}

struct Expr Expr_loop(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Expr body
) {
    struct Expr* loop = serene_trealloc(alloc, struct Expr);
    assert(loop && "OOM");
    *loop = body;
    const struct Type* type = Type_new_typevar(intern);
    return (struct Expr) { .tag = ET_Loop, .type = type, .loop = loop };
}

struct Expr Expr_let(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Binding binding,
    struct Expr init
) {
    struct ExprLet* let = serene_trealloc(alloc, struct ExprLet);
    assert(let && "OOM");
    let->bind = binding;
    let->init = init;
    const struct Type* type = intern->tsyms.t_unit;
    return (struct Expr) { .tag = ST_Let, .type = type, .let = let };
}

struct Expr Expr_mut(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Binding binding,
    struct Expr init
) {
    struct ExprLet* mut = serene_trealloc(alloc, struct ExprLet);
    assert(mut && "OOM");
    mut->bind = binding;
    mut->init = init;
    const struct Type* type = intern->tsyms.t_unit;
    return (struct Expr) { .tag = ST_Mut, .type = type, .let = mut };
}

struct Expr Expr_recall(struct TypeIntern* intern, struct String name) {
    const struct Type* type = Type_new_typevar(intern);
    return (struct Expr){.tag = ET_Recall, .type = type, .lit = name};
}

struct Expr Expr_number(struct TypeIntern* intern, struct String lit) {
    return (struct Expr){.tag = ET_NumberLit, .type = intern->tsyms.t_int, .lit = lit};
}

struct Expr Expr_string(struct TypeIntern* intern, struct String lit) {
    return (struct Expr){.tag = ET_StringLit, .type = intern->tsyms.t_string, .lit = lit};
}

struct Expr Expr_bool(struct TypeIntern* intern, struct String lit) {
    return (struct Expr){.tag = ET_BoolLit, .type = intern->tsyms.t_bool, .lit = lit};
}

struct Expr Expr_assign(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct String name,
    struct Expr value
) {
    struct ExprAssign* assign = serene_trealloc(alloc, struct ExprAssign);
    assert(assign && "OOM");
    assign->name = name;
    assign->expr = value;
    return (struct Expr) { .tag = ST_Assign, .type = intern->tsyms.t_unit, .assign = assign };
}

struct Expr Expr_break(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Expr body
) {
    struct Expr* expr = serene_trealloc(alloc, struct Expr);
    assert(expr && "OOM");
    *expr = body;
    return (struct Expr){.tag = ST_Break, .type = intern->tsyms.t_unit, .break_stmt = expr};
}

struct Expr Expr_return(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Expr body
) {
    struct Expr* expr = serene_trealloc(alloc, struct Expr);
    assert(expr && "OOM");
    *expr = body;
    return (struct Expr){.tag = ST_Return, .type = intern->tsyms.t_unit, .return_stmt = expr};
}

struct Expr Expr_const(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct Expr body
) {
    struct Expr* expr = serene_trealloc(alloc, struct Expr);
    assert(expr && "OOM");
    *expr = body;
    return (struct Expr){.tag = ST_Const, .type = intern->tsyms.t_unit, .const_stmt = expr};
}
