#include "./typer.h"
#include "./common_ll.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// Types are always interned
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

static struct TypeLL *DSet_insert(struct serene_Allocator, struct DSet *, Type);
static struct TypeLL *DSet_root(struct TypeLL *);
static struct TypeLL *DSet_find_root(struct DSet *, Type);
static struct TypeLL *DSet_join(struct TypeLL *, struct TypeLL *);
static void DSet_print(struct DSet *);

struct Globals {
    struct TypeIntern *intern;
    struct serene_Allocator alloc;
    struct GlobalsLL {
        struct {
            const char *name;
            Type type;
        } current;
        struct GlobalsLL *next;
    } *globals;
};

static Type unify(struct serene_Allocator, struct DSet *, Type, Type);
static void typecheck_func(struct Globals, struct Function *);

struct Context {
    struct LetsLL {
        struct {
            const char *name;
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

void typecheck(
    struct serene_Allocator alloc, struct TypeIntern *intern, struct Ast *ast
) {
    struct Globals globals = {0};
    globals.intern = intern;
    globals.alloc = alloc;

    for (ll_iter(f, ast->funcs)) {
        struct GlobalsLL *tmp = serene_alloc(alloc, struct GlobalsLL);
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
}

static void typecheck_func(struct Globals globals, struct Function *func) {
    struct Context ctx = {0};
    ctx.globals = globals;
    destructure_binding(&ctx, &func->args, false);
    ctx.ret = func->ret;
    unify(
        globals.alloc, &ctx.equivs, func->ret, typecheck_expr(&ctx, &func->body)
    );

    DSet_print(&ctx.equivs);
    printf("---interns:");
    TypeIntern_print(globals.intern);
    printf("\n\n");

    fill_expr(&ctx, &func->body);
}

static Type typecheck_ET_If(struct Context *ctx, struct ExprIf *expr),
    typecheck_ET_Loop(struct Context *ctx, struct Expr *body, Type type),
    typecheck_ET_Bareblock(
        struct Context *ctx, struct ExprsLL *body, Type type
    ),
    typecheck_ET_Call(struct Context *ctx, struct ExprCall *expr, Type type),
    typecheck_ET_Recall(struct Context *ctx, const char *lit, Type type),
    typecheck_ST_Assign(
        struct Context *ctx, struct ExprAssign *expr, Type type
    ),
    typecheck_ST_Return(struct Context *ctx, struct Expr *body, Type type),
    typecheck_ST_Break(struct Context *ctx, struct Expr *body, Type type),
    typecheck_ST_Mut(struct Context *ctx, struct ExprLet *let, Type type),
    typecheck_ST_Let(struct Context *ctx, struct ExprLet *let, Type type),
    typecheck_ET_Comma(struct Context *ctx, struct ExprComma *expr, Type type),
    typecheck_ET_Recall(struct Context *ctx, const char *lit, Type type);

static Type typecheck_expr(struct Context *ctx, struct Expr *expr) {
    struct serene_Allocator alloc = ctx->globals.alloc;

#define Case(Tag, ...)                                                         \
    case Tag:                                                                  \
        return typecheck_##Tag(ctx, __VA_ARGS__);

    switch (expr->tag) {
        Case(ET_If, expr->if_expr);
        Case(ET_Loop, expr->loop, expr->type);
        Case(ET_Bareblock, expr->bareblock, expr->type);
        Case(ET_Call, expr->call, expr->type);
        Case(ET_Recall, expr->lit, expr->type);
        Case(ET_Comma, expr->comma, expr->type);

        Case(ST_Let, expr->let, expr->type);
        Case(ST_Mut, expr->let, expr->type);
        Case(ST_Break, expr->break_stmt, expr->type);
        Case(ST_Return, expr->return_stmt, expr->type);
        Case(ST_Assign, expr->assign, expr->type);

    case ST_Const:
        typecheck_expr(ctx, expr->const_stmt);
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
    case ET_Unit:
        return expr->type;
    }

#undef Case

    assert(false && "gcc complains about control reaching here??");
}

static Type typecheck_ET_If(struct Context *ctx, struct ExprIf *expr) {
    Type cond = typecheck_expr(ctx, &expr->cond);
    unify(
        ctx->globals.alloc, &ctx->equivs, cond,
        ctx->globals.intern->tsyms.t_bool
    );
    Type smash = typecheck_expr(ctx, &expr->smash);
    Type pass = typecheck_expr(ctx, &expr->pass);
    return unify(ctx->globals.alloc, &ctx->equivs, smash, pass);
}

static Type
typecheck_ET_Loop(struct Context *ctx, struct Expr *body, Type type) {
    struct LoopsLL *head = serene_alloc(ctx->globals.alloc, struct LoopsLL);
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

static Type
typecheck_ET_Call(struct Context *ctx, struct ExprCall *expr, Type type) {
    Type f, a, r;
    f = typecheck_expr(ctx, &expr->name);
    a = typecheck_expr(ctx, &expr->args);
    r = type;
    Type fa = Type_func(ctx->globals.intern, a, r);
    Type p = unify(ctx->globals.alloc, &ctx->equivs, f, fa);
    assert(p->tag == TT_Func);
    return p->func.ret;
}

static Type
typecheck_ET_Recall(struct Context *ctx, const char *lit, Type type) {
    for (ll_iter(head, ctx->lets)) {
        if (head->current.name == lit) {
            return unify(
                ctx->globals.alloc, &ctx->equivs, type, head->current.type
            );
        }
    }
    for (ll_iter(head, ctx->globals.globals)) {
        if (head->current.name == lit) {
            return unify(
                ctx->globals.alloc, &ctx->equivs, type, head->current.type
            );
        }
    }
    assert(false && "no such name!");
}

static Type
typecheck_ET_Comma(struct Context *ctx, struct ExprComma *expr, Type type) {
    typecheck_expr(ctx, &expr->lhs);
    typecheck_expr(ctx, &expr->rhs);
    return type;
}

static Type
typecheck_ST_Let(struct Context *ctx, struct ExprLet *let, Type type) {
    Type it = typecheck_expr(ctx, &let->init);
    Type bt = destructure_binding(ctx, &let->bind, false);
    return unify(ctx->globals.alloc, &ctx->equivs, it, bt);
}

static Type
typecheck_ST_Mut(struct Context *ctx, struct ExprLet *let, Type type) {
    Type it = typecheck_expr(ctx, &let->init);
    Type bt = destructure_binding(ctx, &let->bind, true);
    return unify(ctx->globals.alloc, &ctx->equivs, it, bt);
}

static Type
typecheck_ST_Break(struct Context *ctx, struct Expr *body, Type type) {
    Type brk = typecheck_expr(ctx, body);
    assert(ctx->loops);
    unify(ctx->globals.alloc, &ctx->equivs, brk, ctx->loops->current);
    return type;
}

static Type
typecheck_ST_Return(struct Context *ctx, struct Expr *body, Type type) {
    Type ret = typecheck_expr(ctx, body);
    unify(ctx->globals.alloc, &ctx->equivs, ret, ctx->ret);
    return type;
}

static Type
typecheck_ST_Assign(struct Context *ctx, struct ExprAssign *expr, Type type) {
    for (ll_iter(head, ctx->lets)) {
        if (head->current.name == expr->name) {
            assert(
                head->current.mutable && "tried modifying an immutable var!"
            );
            Type ass = typecheck_expr(ctx, &expr->expr);
            return unify(
                ctx->globals.alloc, &ctx->equivs, ass, head->current.type
            );
        }
    }
    assert(false && "no such variable declared!");
}

static void fill_ET_If(struct Context *ctx, struct ExprIf *expr),
    fill_ET_Bareblock(struct Context *ctx, struct ExprsLL *body),
    fill_ET_Call(struct Context *ctx, struct ExprCall *expr),
    fill_ET_Comma(struct Context *ctx, struct ExprComma *expr),
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
        Case(ET_Comma, expr->comma);

    case ST_Mut:
        Case(ST_Let, expr->let);

    case ET_Recall:
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
    case ET_Unit:
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

static void fill_ET_Comma(struct Context *ctx, struct ExprComma *expr) {
    fill_expr(ctx, &expr->lhs);
    fill_expr(ctx, &expr->rhs);
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
    case BT_Comma: {
        fill_binding(ctx, binding->comma.lhs);
        fill_binding(ctx, binding->comma.rhs);
        return;
    }
    };
}

static Type fill_TT_Func(struct Context *ctx, struct TypeFunc type),
    fill_TT_Call(struct Context *ctx, struct TypeCall type),
    fill_TT_Comma(struct Context *ctx, struct TypeComma type),
    fill_TT_Var(struct Context *ctx, Type whole);

static Type fill_type(struct Context *ctx, Type type) {
#define Case(Tag, ...)                                                         \
    case Tag:                                                                  \
        return fill_##Tag(ctx, __VA_ARGS__);

    switch (type->tag) {
        Case(TT_Func, type->func);
        Case(TT_Call, type->call);
        Case(TT_Comma, type->comma);
        Case(TT_Var, type);

    case TT_Recall:
        return type;
    }

#undef Case
    assert(false && "shouldn't");
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

static Type fill_TT_Comma(struct Context *ctx, struct TypeComma type) {
    Type lhs = fill_type(ctx, type.lhs);
    Type rhs = fill_type(ctx, type.rhs);
    return Type_comma(ctx->globals.intern, lhs, rhs);
}

static Type fill_TT_Var(struct Context *ctx, Type whole) {
    const struct TypeLL *t = DSet_find_root(&ctx->equivs, whole);
    assert(t && "Idek");
    whole = t->current.type;
    assert(whole->tag != TT_Var && "it seems not all types are resolved!");
    return fill_type(ctx, whole);
}

static Type
destructure_binding(struct Context *ctx, struct Binding *binding, bool mut) {
    switch (binding->tag) {
    case BT_Empty:
        return binding->empty;
    case BT_Name: {
        struct LetsLL *tmp = serene_alloc(ctx->globals.alloc, struct LetsLL);
        assert(tmp);
        tmp->current.name = binding->name.name;
        tmp->current.mutable = mut;
        tmp->current.type = binding->name.annot;
        tmp->next = ctx->lets;
        ctx->lets = tmp;
        return binding->name.annot;
    }
    case BT_Comma: {
        Type lhs = destructure_binding(ctx, binding->comma.lhs, mut);
        Type rhs = destructure_binding(ctx, binding->comma.rhs, mut);
        return Type_product(ctx->globals.intern, lhs, rhs);
    }
    }
    assert(false && "shouldn't");
}

static Type
unify(struct serene_Allocator alloc, struct DSet *dset, Type lhs, Type rhs) {
    struct TypeLL *lroot = DSet_root(DSet_insert(alloc, dset, lhs));
    Type ltype = lroot->current.type;
    struct TypeLL *rroot = DSet_root(DSet_insert(alloc, dset, rhs));
    Type rtype = rroot->current.type;
    if (ltype->tag == rtype->tag) {
        switch (ltype->tag) {
        case TT_Call:
            unify(alloc, dset, ltype->call.name, rtype->call.name);
            unify(alloc, dset, ltype->call.args, rtype->call.args);
            return lroot->current.type;
        case TT_Func:
            unify(alloc, dset, ltype->func.args, rtype->func.args);
            unify(alloc, dset, ltype->func.ret, rtype->func.ret);
            return ltype;
        case TT_Comma:
            unify(alloc, dset, ltype->comma.lhs, rtype->comma.lhs);
            unify(alloc, dset, ltype->comma.rhs, rtype->comma.rhs);
            return ltype;
        case TT_Recall:
            assert(ltype->recall == rtype->recall && "type mismatch");
            return ltype;
        case TT_Var:
            break;
        }
    } else {
        assert(
            (ltype->tag == TT_Var || rtype->tag == TT_Var) && "type mismatch"
        );
    }
    return DSet_join(lroot, rroot)->current.type;
}

static struct TypeLL *
DSet_insert(struct serene_Allocator alloc, struct DSet *this, Type type) {
    for (ll_iter(head, this->types)) {
        if (head->current.type == type)
            return head;
    }

    printf("inserting type: (%p) ", type);
    Type_print(type);
    printf("\n");

    struct TypeLL *tmp = serene_alloc(alloc, struct TypeLL);
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
