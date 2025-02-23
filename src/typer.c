#include "./typer.h"
#include "./common_ll.h"
#include "commons.h"
#include "parser.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef const struct Type *Type;

struct DSet {
    struct TypeLL {
        struct {
            struct TypeLL *parent;
            Type type;
        } current;
        struct TypeLL *next;
    } *types;
};

static struct TypeLL *DSet_insert(struct serene_Trea*, struct DSet *, Type);
static struct TypeLL *DSet_root(struct TypeLL *);
static struct TypeLL *DSet_find_root(struct DSet *, Type);
static struct TypeLL *DSet_join(struct TypeLL *, struct TypeLL *);
static void DSet_print(struct DSet *);

struct Globals {
    struct TypeIntern* intern;
    struct serene_Trea* alloc;
    struct GlobalsLL {
        struct GlobalsLL *next;
        struct {
            struct String name;
            Type type;
        } current;
    } *globals;
};

static Type unify(struct serene_Trea*, struct TypeIntern*, struct DSet *, Type, Type);
static void typecheck_func(struct Globals, struct Function *);

struct Context {
    struct LetsLL {
        struct {
            struct String name;
            bool mutable;
            Type type;
        } current;
        struct LetsLL *next;
    } *lets;
    struct LoopsLL {
        struct LoopsLL *next;
        Type current;
    } *loops;
    Type ret;
    struct DSet equivs;
    struct Globals globals;
};

static Type typecheck_expr(struct Context *, struct Expr *);
static void fill_expr(struct Context *, struct Expr *);
static void fill_binding(struct Context *, struct Binding *);
static Type fill_type(struct Context *, Type);
static Type destructure_binding(struct Context *, struct Binding *, bool);

static void typecheck_top(struct TypeIntern*, struct Ast*, struct PPImports*);

static void* typecheck_ptr(void* _ctx, void* _data) {
    (void)_ctx;
    struct PTData* data = _data;
    if (!data) return data;
    typecheck_top(&data->types, &data->ast, data->imports);
    return data;
}

static void cleanup(void* _data) {
    (void)_data;
}

void typecheck(struct MTree* mods) {
    MTree_map(mods, cleanup, typecheck_ptr, NULL);
}

static void typecheck_top(
    struct TypeIntern* intern,
    struct Ast* ast,
    struct PPImports* imports
) {
    struct serene_Trea alloc = serene_Trea_sub(intern->alloc);
    struct Globals globals = {0};
    globals.intern = intern;
    globals.alloc = &alloc;

    {
        Type t_int = intern->tsyms.t_int;
        Type t_bool = intern->tsyms.t_bool;
        Type t_string = intern->tsyms.t_string;
        Type string_to_int = Type_func(intern, t_string, t_int);
        Type int_to_string = Type_func(intern, t_int, t_string);
        struct String t_name = intern->syms.s_main;
        Type t_type = Type_recall(intern, t_name);
        Type t2 = Type_call(intern, intern->tsyms.t_star, Type_tuple(intern, t_type, t_type));
        Type forall_t2_to_t = Type_forall(
            intern,
            t_name,
            Type_func(
                intern,
                t2,
                t_type
            )
        );
        Type forall_t2_to_bool = Type_forall(
            intern,
            t_name,
            Type_func(
                intern,
                t2,
                t_bool
            )
        );
        Type forall_t_to_t = Type_forall(
            intern,
            t_name,
            Type_func(intern, t_type, t_type)
        );
        Type int7_to_int;
        {
            Type int2 = Type_tuple_extend(globals.intern, t_int, t_int);
            Type int3 = Type_tuple_extend(globals.intern, int2, t_int);
            Type int4 = Type_tuple_extend(globals.intern, int3, t_int);
            Type int5 = Type_tuple_extend(globals.intern, int4, t_int);
            Type int6 = Type_tuple_extend(globals.intern, int5, t_int);
            Type int7 = Type_tuple_extend(globals.intern, int6, t_int);
            Type tint7 = Type_call(globals.intern, globals.intern->tsyms.t_star, int7);
            int7_to_int = Type_func(globals.intern, tint7, t_int);
        }
        struct {
            struct String name;
            Type type;
        } builtins[] = {
            {.name = intern->syms.s_badd, .type = forall_t2_to_t},
            {.name = intern->syms.s_bsub, .type = forall_t2_to_t},
            {.name = intern->syms.s_bmul, .type = forall_t2_to_t},
            {.name = intern->syms.s_bdiv, .type = forall_t2_to_t},
            {.name = intern->syms.s_bmod, .type = forall_t2_to_t},
            {.name = intern->syms.s_band, .type = forall_t2_to_t},
            {.name = intern->syms.s_bor, .type = forall_t2_to_t},
            {.name = intern->syms.s_bxor, .type = forall_t2_to_t},
            {.name = intern->syms.s_bshl, .type = forall_t2_to_t},
            {.name = intern->syms.s_bshr, .type = forall_t2_to_t},
            {.name = intern->syms.s_bnot, .type = forall_t_to_t},
            {.name = intern->syms.s_bneg, .type = forall_t_to_t},
            {.name = intern->syms.s_bcmpEQ, .type = forall_t2_to_bool},
            {.name = intern->syms.s_bcmpNE, .type = forall_t2_to_bool},
            {.name = intern->syms.s_bcmpGT, .type = forall_t2_to_bool},
            {.name = intern->syms.s_bcmpLT, .type = forall_t2_to_bool},
            {.name = intern->syms.s_bcmpGE, .type = forall_t2_to_bool},
            {.name = intern->syms.s_bcmpLE, .type = forall_t2_to_bool},
            {.name = intern->syms.s_syscall, .type = int7_to_int},
            // currently only strings are ptrs
            // in the future this should become a generic builtin
            {.name = intern->syms.s_bptr_to_int, .type = string_to_int},
            {.name = intern->syms.s_bint_to_ptr, .type = int_to_string},
        };
        for (unsigned int i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
            struct GlobalsLL* tmp = serene_trealloc(&alloc, struct GlobalsLL);
            assert(tmp && "OOM");
            tmp->current.type = builtins[i].type;
            tmp->current.name = builtins[i].name;
            tmp->next = globals.globals;
            globals.globals = tmp;
        }
    }

    for (ll_iter(i, imports)) {
        struct PTData* data = i->current.mod->data;
        if (i->current.decl.len == 0) {
            assert(false && "TODO: import module symbol");
            continue;
        }
        if (strings_equal(i->current.decl, (struct String){"...", 3})) {
            for (ll_iter(f, data->ast.funcs)) {
                struct GlobalsLL *tmp = serene_trealloc(&alloc, struct GlobalsLL);
                assert(tmp);

                Type ret = f->current.ret;
                Type args = Binding_to_type(globals.intern, f->current.args);
                tmp->current.type = Type_func(globals.intern, args, ret);
                tmp->current.name = f->current.name;
                tmp->next = globals.globals;
                globals.globals = tmp;
            }
            continue;
        }
        for (ll_iter(f, data->ast.funcs)) {
            if (f->current.name.str != i->current.decl.str) continue;

            struct GlobalsLL *tmp = serene_trealloc(&alloc, struct GlobalsLL);
            assert(tmp && "OOM"), ZERO(*tmp);
            Type ret = f->current.ret;
            Type args = Binding_to_type(globals.intern, f->current.args);
            tmp->current.type = Type_func(globals.intern, args, ret);
            tmp->current.name = f->current.name;
            tmp->next = globals.globals;
            globals.globals = tmp;
            break;
        }
    }

    for (ll_iter(f, ast->funcs)) {
        struct GlobalsLL *tmp = serene_trealloc(&alloc, struct GlobalsLL);
        assert(tmp);

        Type ret = f->current.ret;
        Type args = Binding_to_type(globals.intern, f->current.args);
        tmp->current.type = Type_func(globals.intern, args, ret);
        tmp->current.name = f->current.name;
        tmp->next = globals.globals;
        globals.globals = tmp;
    }

    for (ll_iter(f, ast->funcs)) {
        typecheck_func(globals, &f->current);
    }

    serene_Trea_deinit(alloc);
}

static void typecheck_func(struct Globals globals, struct Function *func) {
    struct Context ctx = {0};
    ctx.globals = globals;
    destructure_binding(&ctx, &func->args, false);
    ctx.ret = func->ret;
    unify(
        globals.alloc, ctx.globals.intern, &ctx.equivs, func->ret, typecheck_expr(&ctx, &func->body)
    );

    DSet_print(&ctx.equivs);
    printf("---interns:");
    TypeIntern_print(globals.intern);
    printf("\n\n");

    fill_expr(&ctx, &func->body);
}

static Type typecheck_ET_If(struct Context* ctx, struct ExprIf* expr),
    typecheck_ET_Loop(struct Context* ctx, struct Expr* body, Type type),
    typecheck_ET_Bareblock(
        struct Context* ctx,
        struct ExprsLL* body,
        Type type
    ),
    typecheck_ET_Call(struct Context* ctx, struct ExprCall* expr, Type type),
    typecheck_ET_Recall(struct Context* ctx, struct String lit, Type type),
    typecheck_ST_Assign(
        struct Context *ctx, struct ExprAssign *expr, Type type
    ),
    typecheck_ST_Return(struct Context *ctx, struct Expr *body, Type type),
    typecheck_ST_Break(struct Context *ctx, struct Expr *body, Type type),
    typecheck_ST_Mut(struct Context *ctx, struct ExprLet *let, Type type),
    typecheck_ST_Let(struct Context *ctx, struct ExprLet *let, Type type),
    typecheck_ET_Tuple(struct Context *ctx, struct ExprTuple expr, Type type),
    typecheck_ET_Recall(struct Context *ctx, struct String lit, Type type);

static Type typecheck_expr(struct Context *ctx, struct Expr *expr) {
#define Case(Tag, ...)                                                         \
    case Tag:                                                                  \
        return typecheck_##Tag(ctx, __VA_ARGS__);

    switch (expr->tag) {
        Case(ET_If, expr->if_expr);
        Case(ET_Loop, expr->loop, expr->type);
        Case(ET_Bareblock, expr->bareblock, expr->type);
        Case(ET_Call, expr->call, expr->type);
        Case(ET_Recall, expr->lit, expr->type);
        Case(ET_Tuple, expr->tuple, expr->type);

        Case(ST_Let, expr->let, expr->type);
        Case(ST_Mut, expr->let, expr->type);
        Case(ST_Break, expr->break_stmt, expr->type);
        Case(ST_Return, expr->return_stmt, expr->type);
        Case(ST_Assign, expr->assign, expr->type);

    case ST_Const:
        typecheck_expr(ctx, expr->const_stmt);
        __attribute__((fallthrough));
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
        return expr->type;
    }

#undef Case

    assert(false && "gcc complains about control reaching here??");
}

static Type typecheck_ET_If(struct Context *ctx, struct ExprIf *expr) {
    Type cond = typecheck_expr(ctx, &expr->cond);
    unify(
        ctx->globals.alloc,
        ctx->globals.intern,
        &ctx->equivs, cond,
        ctx->globals.intern->tsyms.t_bool
    );
    Type smash = typecheck_expr(ctx, &expr->smash);
    Type pass = typecheck_expr(ctx, &expr->pass);
    return unify(ctx->globals.alloc, ctx->globals.intern, &ctx->equivs, smash, pass);
}

static Type typecheck_ET_Loop(struct Context *ctx, struct Expr *body, Type type) {
    struct LoopsLL *head = serene_trealloc(ctx->globals.alloc, struct LoopsLL);
    head->current = type;
    head->next = ctx->loops;
    ctx->loops = head;
    typecheck_expr(ctx, body);
    ctx->loops = head->next;
    return type;
}

static Type
typecheck_ET_Bareblock(struct Context *ctx, struct ExprsLL *body, Type type) {
    // TODO: we should probably also revert arena state
    // so as to free up the memory that is 'leaked' here
    // that is however complicated, as the arena is used
    // for many things beyond ctx->lets
    struct LetsLL *head = ctx->lets;
    for (ll_iter(s, body)) {
        typecheck_expr(ctx, &s->current);
    }
    ctx->lets = head;
    return type;
}

static Type typecheck_ET_Call(struct Context* ctx, struct ExprCall* expr, Type type) {
    Type f, a, r;
    f = typecheck_expr(ctx, &expr->name);
    a = typecheck_expr(ctx, &expr->args);
    r = type;
    Type fa = Type_func(ctx->globals.intern, a, r);
    Type p = unify(ctx->globals.alloc, ctx->globals.intern, &ctx->equivs, f, fa);
    assert(p->tag == TT_Func);
    return p->func.ret;
}

static Type typecheck_ET_Recall(struct Context *ctx, struct String lit, Type type) {
    for (ll_iter(head, ctx->lets)) {
        if (head->current.name.str == lit.str) {
            return unify(
                ctx->globals.alloc,
                ctx->globals.intern,
                &ctx->equivs, type, head->current.type
            );
        }
    }
    for (ll_iter(head, ctx->globals.globals)) {
        if (head->current.name.str == lit.str) {
            return unify(
                ctx->globals.alloc,
                ctx->globals.intern,
                &ctx->equivs, type, head->current.type
            );
        }
    }
    printf("\nno name such as '%.*s'!\n", (int) lit.len, lit.str);
    assert(false);
}

static Type typecheck_ET_Tuple(
    struct Context* ctx,
    struct ExprTuple expr,
    Type type
) {
    for (ll_iter(head, expr.list)) {
        typecheck_expr(ctx, &head->current);
    }
    return type;
}

static Type typecheck_ST_Let(struct Context* ctx, struct ExprLet* let, Type type) {
    (void) type;
    Type it = typecheck_expr(ctx, &let->init);
    Type bt = destructure_binding(ctx, &let->bind, false);
    return unify(ctx->globals.alloc, ctx->globals.intern, &ctx->equivs, it, bt);
}

static Type typecheck_ST_Mut(struct Context* ctx, struct ExprLet* let, Type type) {
    (void) type;
    Type it = typecheck_expr(ctx, &let->init);
    Type bt = destructure_binding(ctx, &let->bind, true);
    return unify(ctx->globals.alloc, ctx->globals.intern, &ctx->equivs, it, bt);
}

static Type typecheck_ST_Break(struct Context *ctx, struct Expr *body, Type type) {
    Type brk = typecheck_expr(ctx, body);
    assert(ctx->loops);
    unify(ctx->globals.alloc, ctx->globals.intern, &ctx->equivs, brk, ctx->loops->current);
    return type;
}

static Type typecheck_ST_Return(struct Context *ctx, struct Expr *body, Type type) {
    Type ret = typecheck_expr(ctx, body);
    unify(ctx->globals.alloc, ctx->globals.intern, &ctx->equivs, ret, ctx->ret);
    return type;
}

static Type typecheck_ST_Assign(
    struct Context* ctx,
    struct ExprAssign* expr,
    Type type
) {
    (void)type;
    for (ll_iter(head, ctx->lets)) {
        if (head->current.name.str == expr->name.str) {
            assert(
                head->current.mutable && "tried modifying an immutable var!"
            );
            Type ass = typecheck_expr(ctx, &expr->expr);
            return unify(
                ctx->globals.alloc, ctx->globals.intern, &ctx->equivs, ass, head->current.type
            );
        }
    }
    assert(false && "no such variable declared!");
}

static void fill_ET_If(struct Context* ctx, struct ExprIf* expr),
    fill_ET_Bareblock(struct Context* ctx, struct ExprsLL* body),
    fill_ET_Call(struct Context* ctx, struct ExprCall* expr),
    fill_ET_Tuple(struct Context* ctx, struct ExprTuple* expr),
    fill_ST_Let(struct Context *ctx, struct ExprLet *let);

static void fill_expr(struct Context *ctx, struct Expr *expr) {
    expr->type = fill_type(ctx, expr->type);

#define Case(Tag, ...)                                                         \
    case Tag:                                                                  \
        fill_##Tag(ctx, __VA_ARGS__);                                          \
        break;

    switch (expr->tag) {
        Case(ET_If, expr->if_expr);
        Case(ET_Bareblock, expr->bareblock);
        Case(ET_Call, expr->call);
        Case(ET_Tuple, &expr->tuple);

    case ST_Mut:
        Case(ST_Let, expr->let);

    case ET_Recall:
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
        break;
    case ST_Break:
        fill_expr(ctx, expr->break_stmt);
        break;
    case ST_Return:
        fill_expr(ctx, expr->return_stmt);
        break;
    case ST_Const:
        fill_expr(ctx, expr->const_stmt);
        break;
    case ST_Assign:
        fill_expr(ctx, &expr->assign->expr);
        break;
    case ET_Loop:
        fill_expr(ctx, expr->loop);
        break;
    }

#undef Case
}

static void fill_ET_If(struct Context *ctx, struct ExprIf *expr) {
    fill_expr(ctx, &expr->cond);
    fill_expr(ctx, &expr->smash);
    fill_expr(ctx, &expr->pass);
}

static void fill_ET_Bareblock(struct Context *ctx, struct ExprsLL *body) {
    for (ll_iter(head, body)) {
        fill_expr(ctx, &head->current);
    }
}

static void fill_ET_Call(struct Context *ctx, struct ExprCall *expr) {
    fill_expr(ctx, &expr->name);
    fill_expr(ctx, &expr->args);
}

static void fill_ET_Tuple(struct Context* ctx, struct ExprTuple* expr) {
    for (ll_iter(head, expr->list)) {
        fill_expr(ctx, &head->current);
    }
}

static void fill_ST_Let(struct Context *ctx, struct ExprLet *let) {
    fill_expr(ctx, &let->init);
    fill_binding(ctx, &let->bind);
}

static void fill_binding(struct Context *ctx, struct Binding *binding) {
    switch (binding->tag) {
    case BT_Empty:
        return;
    case BT_Name: {
        binding->name.annot = fill_type(ctx, binding->name.annot);
        return;
    }
    case BT_Tuple: {
        for (ll_iter(head, binding->tuple)) {
            fill_binding(ctx, &head->current);
        }
        return;
    }
    };
}

static Type fill_TT_Func(struct Context* ctx, struct TypeFunc type),
    fill_TT_Call(struct Context* ctx, struct TypeCall type),
    fill_TT_Tuple(struct Context* ctx, const struct TypeTuple* type),
    fill_TT_Forall(struct Context* ctx, const struct TypeForall* type),
    fill_TT_Var(struct Context *ctx, Type whole);

static Type fill_type(struct Context *ctx, Type type) {
#define Case(Tag, ...)                                                         \
    case Tag:                                                                  \
        return fill_##Tag(ctx, __VA_ARGS__);

    switch (type->tag) {
        Case(TT_Forall, &type->forall);
        Case(TT_Func, type->func);
        Case(TT_Call, type->call);
        Case(TT_Tuple, type->tuple);
        Case(TT_Var, type);

    case TT_Recall:
        return type;
    }

#undef Case
    assert(false && "shouldn't");
}

static Type fill_TT_Forall(struct Context* ctx, const struct TypeForall* type) {
    Type in = fill_type(ctx, type->in);
    return Type_forall(ctx->globals.intern, type->binding, in);
}

static Type fill_TT_Func(struct Context *ctx, struct TypeFunc type) {
    Type args = fill_type(ctx, type.args);
    Type ret = fill_type(ctx, type.ret);
    return Type_func(ctx->globals.intern, args, ret);
}

static Type fill_TT_Call(struct Context *ctx, struct TypeCall type) {
    Type name = fill_type(ctx, type.name);
    Type args = fill_type(ctx, type.args);
    return Type_call(ctx->globals.intern, name, args);
}

static Type fill_TT_Tuple(struct Context* ctx, const struct TypeTuple* type) {
    struct TypeTuple* list = NULL;
    struct TypeTuple* last = NULL;
    for (ll_iter(head, type)) {
        struct TypeTuple* tmp = serene_trealloc(ctx->globals.alloc, struct TypeTuple);
        assert(tmp && "OOM");
        *tmp = (struct TypeTuple){0};
        tmp->current = fill_type(ctx, head->current);
        if (!last) list = tmp;
        else last->next = tmp;
        last = tmp;
    }
    return TypeIntern_intern(ctx->globals.intern, &(struct Type){.tag = TT_Tuple, .tuple = list});
}

static Type fill_TT_Var(struct Context *ctx, Type whole) {
    const struct TypeLL *t = DSet_find_root(&ctx->equivs, whole);
    assert(t && "Idek");
    whole = t->current.type;
    assert(whole->tag != TT_Var && "it seems not all types are resolved!");
    return fill_type(ctx, whole);
}

static Type destructure_binding(
    struct Context* ctx,
    struct Binding* binding,
    bool mut
) {
    switch (binding->tag) {
        case BT_Empty:
            return binding->empty;
        case BT_Name: {
            struct LetsLL* tmp = serene_trealloc(ctx->globals.alloc, struct LetsLL);
            assert(tmp);
            tmp->current.name = binding->name.name;
            tmp->current.mutable = mut;
            tmp->current.type = binding->name.annot;
            tmp->next = ctx->lets;
            ctx->lets = tmp;
            return binding->name.annot;
        }
        case BT_Tuple: {
            Type out = ctx->globals.intern->tsyms.t_unit;
            for (ll_iter(head, binding->tuple)) {
                Type current = destructure_binding(ctx, &head->current, mut);
                out = Type_tuple_extend(ctx->globals.intern, out, current);
            }
            return Type_call(ctx->globals.intern, ctx->globals.intern->tsyms.t_star, out);
        }
    }
    assert(false && "shouldn't");
}

static Type index_generics(
    struct TypeIntern* intern,
    Type body,
    struct String name,
    Type arg
) {
    switch (body->tag) {
    case TT_Forall: assert(false && "why");
    case TT_Func: {
        Type args = index_generics(intern, body->func.args, name, arg);
        Type ret = index_generics(intern, body->func.ret, name, arg);
        return Type_func(intern, args, ret);
    }
    case TT_Call: {
        Type _name = index_generics(intern, body->call.name, name, arg);
        Type args = index_generics(intern, body->call.args, name, arg);
        return Type_call(intern, _name, args);
    }
    case TT_Recall: {
        if (strings_equal(name, body->recall)) return arg;
        return body;
    }
    case TT_Tuple: {
        struct TypeTuple* tuple = NULL;
        struct TypeTuple* last = tuple;
        for (const struct TypeTuple* head = body->tuple; head; head = head->next) {
            struct TypeTuple* tmp = serene_trealloc(intern->alloc, struct TypeTuple);
            assert(tmp && "OOM"), ZERO(*tmp);
            tmp->current = index_generics(intern, head->current, name, arg);
            if (!last) tuple = tmp;
            else last->next = tmp;
            last = tmp;
        }
        struct Type out = {
            .tag = TT_Tuple,
            .tuple = tuple,
        };
        return TypeIntern_intern(intern, &out);
    }
    case TT_Var: return body;
    }
    assert(false && "what why");
}

static Type unify(
    struct serene_Trea* alloc,
    struct TypeIntern* intern,
    struct DSet* dset,
    Type lhs,
    Type rhs
) {
    struct TypeLL* lroot = DSet_root(DSet_insert(alloc, dset, lhs));
    Type ltype = lroot->current.type;
    struct TypeLL *rroot = DSet_root(DSet_insert(alloc, dset, rhs));
    Type rtype = rroot->current.type;
    if (rtype->tag == TT_Forall) {
        Type tmp = rtype;
        rtype = ltype;
        ltype = tmp;
    }
    if (ltype->tag == rtype->tag || ltype->tag == TT_Forall) {
        switch (ltype->tag) {
            case TT_Forall: {
                assert(rtype->tag != TT_Forall && "why");
                ltype = index_generics(
                    intern,
                    ltype->forall.in,
                    ltype->forall.binding,
                    Type_new_typevar(intern)
                    );
                return unify(alloc, intern, dset, ltype, rtype);
            }
            case TT_Call:
                unify(alloc, intern, dset, ltype->call.name, rtype->call.name);
                unify(alloc, intern, dset, ltype->call.args, rtype->call.args);
                return lroot->current.type;
            case TT_Func:
                unify(alloc, intern, dset, ltype->func.args, rtype->func.args);
                unify(alloc, intern, dset, ltype->func.ret, rtype->func.ret);
                return ltype;
            case TT_Tuple: {
                const struct TypeTuple* lhead = ltype->tuple;
                const struct TypeTuple* rhead = rtype->tuple;
                while (lhead && rhead) {
                    unify(alloc, intern, dset, lhead->current, rhead->current);
                    lhead = lhead->next;
                    rhead = rhead->next;
                }
                assert(lhead == rhead && "type mismatch");
                return ltype;
            }
            case TT_Recall:
                assert(ltype->recall.str == rtype->recall.str && "type mismatch");
                return ltype;
            case TT_Var:
                break;
        }
    } else {
        Type_print(ltype);
        printf("\n");
        Type_print(rtype);
        printf("\n");
        assert(
            (ltype->tag == TT_Var || rtype->tag == TT_Var) && "type mismatch"
        );
    }
    return DSet_join(lroot, rroot)->current.type;
}

static struct TypeLL* DSet_insert(struct serene_Trea* alloc, struct DSet *this, Type type) {
    for (ll_iter(head, this->types)) {
        if (head->current.type == type)
            return head;
    }

    printf("inserting type: (%p) ", type);
    Type_print(type);
    printf("\n");

    struct TypeLL *tmp = serene_trealloc(alloc, struct TypeLL);
    assert(tmp && "OOM");
    tmp->next = this->types;
    tmp->current.parent = tmp;
    tmp->current.type = type;
    this->types = tmp;
    return this->types;
}

static struct TypeLL *DSet_root(struct TypeLL *type) {
    while (type->current.parent != type) {
        struct TypeLL *tmp = type;
        type = type->current.parent;
        tmp->current.parent = tmp->current.parent->current.parent;
    }
    return type;
}

static struct TypeLL *DSet_find_root(struct DSet *this, Type type) {
    for (ll_iter(head, this->types)) {
        if (head->current.type == type)
            return DSet_root(head);
    }
    printf("couldn't find type: (%p) ", type);
    Type_print(type);
    printf("\n");
    return NULL;
}

static struct TypeLL *DSet_join(struct TypeLL *lhs, struct TypeLL *rhs) {
    struct TypeLL *lhr = DSet_root(lhs);
    struct TypeLL *rhr = DSet_root(rhs);
    if (lhr->current.type->tag == TT_Var) {
        struct TypeLL *tmp = lhr;
        lhr = rhr;
        rhr = tmp;
    }
    rhr->current.parent = lhr;
    return lhr;
}

static void DSet_print(struct DSet *this) {
    printf("---DSet:\n");
    for (ll_iter(h, this->types)) {
        printf("[%p] (%p) ", h, h->current.type);
        Type_print(h->current.type);
        printf("\t^ (%p)\n", h->current.parent);
    }
}
