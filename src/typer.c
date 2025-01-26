#include "./typer.h"
#include "./common_ll.h"
#include <assert.h>
#include <stdbool.h>

static bool Type_equal(struct Type, struct Type);

struct DSet {
    struct TypeLL {
        struct {
            struct TypeLL *parent;
            const struct Type *type;
        } current;
        struct TypeLL *next;
    } *types;
};

static struct TypeLL *
DSet_insert(struct serene_Allocator, struct DSet *, const struct Type *);
static struct TypeLL *DSet_root(struct TypeLL *);
static struct TypeLL *DSet_find_root(struct DSet *, const struct Type *);
static struct TypeLL *DSet_join(struct TypeLL *, struct TypeLL *);

struct Globals {
    struct TypeIntern *intern;
    struct serene_Allocator alloc;
    struct GlobalsLL {
        struct {
            const char *name;
            const struct Type *type;
        } current;
        struct GlobalsLL *next;
    } *globals;
};

static const struct Type *
unify(struct serene_Allocator, struct DSet *, const struct Type *, const struct Type *);
static void typecheck_func(struct Globals, struct Function *);

struct Context {
    struct LetsLL {
        struct {
            const char *name;
            bool mutable;
            const struct Type *type;
        } current;
        struct LetsLL *next;
    } *lets;
    struct LoopsLL {
        const struct Type *current;
        struct LoopsLL *next;
    } *loops;
    struct DSet equivs;
    const struct Type *ret;
    struct Globals globals;
};

static const struct Type *typecheck_expr(struct Context *, struct Expr *);
static void fill_expr(struct Context *, struct Expr *);
static void fill_binding(struct Context *, struct Binding *);
static const struct Type *fill_type(struct Context *, const struct Type *);
static const struct Type *destructure_binding(struct Context *, struct Binding *, bool);

void typecheck(
    struct serene_Allocator alloc, struct TypeIntern *intern, struct Ast *ast
) {
    struct Globals globals = {0};
    globals.intern = intern;
    globals.alloc = alloc;

    for (ll_iter(f, ast->funcs)) {
        struct GlobalsLL *tmp = serene_alloc(alloc, struct GlobalsLL);
        assert(tmp);

        const struct Type *ret = f->current.ret;
        const struct Type *args =
            Binding_to_type(globals.intern, f->current.args);
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
        globals.alloc, &ctx.equivs, func->ret,
        typecheck_expr(&ctx, &func->body)
    );

    fill_expr(&ctx, &func->body);
}

static const struct Type *typecheck_expr(struct Context *ctx, struct Expr *expr) {
    struct serene_Allocator alloc = ctx->globals.alloc;
    switch (expr->tag) {
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
    case ET_Unit:
        return expr->type;

    case ET_If: {
        const struct Type *cond = typecheck_expr(ctx, expr->if_expr.cond);
        unify(alloc, &ctx->equivs, cond, ctx->globals.intern->tsyms.t_bool);
        const struct Type *smash = typecheck_expr(ctx, expr->if_expr.smash);
        const struct Type *pass = typecheck_expr(ctx, expr->if_expr.pass);
        return unify(alloc, &ctx->equivs, smash, pass);
    }
    case ET_Loop: {
        const struct Type *out = expr->type;
        struct LoopsLL *head = serene_alloc(alloc, struct LoopsLL);
        head->current = out;
        head->next = ctx->loops;
        ctx->loops = head;
        typecheck_expr(ctx, expr->loop);
        ctx->loops = head->next;
        return out;
    }
    case ET_Bareblock: {
        // TODO: we should probably also revert arena state
        // so as to free up the memory that is 'leaked' here
        // that is however complicated, as the arena is used
        // for many things beyond ctx->lets
        struct LetsLL *head = ctx->lets;
        for (ll_iter(s, expr->bareblock)) {
            typecheck_expr(ctx, &s->current);
        }
        ctx->lets = head;
        return expr->type;
    }
    case ET_Call: {
        const struct Type *f;
        const struct Type *a;
        const struct Type *r;
        f = typecheck_expr(ctx, expr->call.name);
        a = typecheck_expr(ctx, expr->call.args);
        r = expr->type;
        const struct Type *fa = Type_func(ctx->globals.intern, a, r);
        const struct Type *p = unify(alloc, &ctx->equivs, f, fa);
        assert(p->tag == TT_Func);
        return p->func.ret;
    }
    case ET_Recall: {
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name == expr->lit) {
                return unify(
                    alloc, &ctx->equivs, expr->type, head->current.type
                );
            }
        }
        for (ll_iter(head, ctx->globals.globals)) {
            if (head->current.name == expr->lit) {
                return unify(
                    alloc, &ctx->equivs, expr->type, head->current.type
                );
            }
        }
        assert(false && "no such name!");
    }
    case ET_Comma: {
        typecheck_expr(ctx, expr->comma.lhs);
        typecheck_expr(ctx, expr->comma.rhs);
        return expr->type;
    }

    case ST_Let: {
        const struct Type *it = typecheck_expr(ctx, expr->let.init);
        const struct Type *bt = destructure_binding(ctx, &expr->let.bind, false);
        return unify(alloc, &ctx->equivs, it, bt);
    }
    case ST_Mut: {
        const struct Type *it = typecheck_expr(ctx, expr->let.init);
        const struct Type *bt = destructure_binding(ctx, &expr->let.bind, true);
        return unify(alloc, &ctx->equivs, it, bt);
    }
    case ST_Break: {
        const struct Type *brk = typecheck_expr(ctx, expr->break_stmt);
        assert(ctx->loops);
        unify(alloc, &ctx->equivs, brk, ctx->loops->current);
        return expr->type;
    }
    case ST_Return: {
        const struct Type *ret = typecheck_expr(ctx, expr->return_stmt);
        unify(alloc, &ctx->equivs, ret, ctx->ret);
        return expr->type;
    }
    case ST_Assign:
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name == expr->assign.name) {
                assert(
                    head->current.mutable && "tried modifying an immutable var!"
                );
                const struct Type *ass = typecheck_expr(ctx, expr->assign.expr);
                return unify(alloc, &ctx->equivs, ass, head->current.type);
            }
        }
        assert(false && "no such variable declared!");
    case ST_Const:
        typecheck_expr(ctx, expr->const_stmt);
        return expr->type;
    }
    assert(false && "gcc complains about control reaching here??");
}

static void fill_expr(struct Context *ctx, struct Expr *expr) {
    expr->type = fill_type(ctx, expr->type);

    switch (expr->tag) {
    case ET_Recall:
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
    case ET_Unit:
        break;

    case ET_If:
        fill_expr(ctx, expr->if_expr.cond);
        fill_expr(ctx, expr->if_expr.smash);
        fill_expr(ctx, expr->if_expr.pass);
        break;
    case ET_Loop:
        fill_expr(ctx, expr->loop);
        break;
    case ET_Bareblock:
        for (ll_iter(head, expr->bareblock)) {
            fill_expr(ctx, &head->current);
        }
        break;
    case ET_Call:
        fill_expr(ctx, expr->call.name);
        fill_expr(ctx, expr->call.args);
        break;
    case ET_Comma:
        fill_expr(ctx, expr->comma.lhs);
        fill_expr(ctx, expr->comma.rhs);
        break;

    case ST_Mut:
    case ST_Let:
        fill_expr(ctx, expr->let.init);
        fill_binding(ctx, &expr->let.bind);
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
        fill_expr(ctx, expr->assign.expr);
        break;
    }
}

static void fill_binding(struct Context *ctx, struct Binding *binding) {
    switch (binding->tag) {
    case BT_Empty: return;
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

static const struct Type *fill_type(struct Context *ctx, const struct Type *type) {
    switch (type->tag) {
    case TT_Func: {
        const struct Type *args = fill_type(ctx, type->func.args);
        const struct Type *ret = fill_type(ctx, type->func.ret);
        return Type_func(ctx->globals.intern, args, ret);
    }
    case TT_Call: {
        const struct Type *name = fill_type(ctx, type->call.name);
        const struct Type *args = fill_type(ctx, type->call.args);
        return Type_call(ctx->globals.intern, name, args);
    }
    case TT_Recall:
        return type;
    case TT_Comma: {
        const struct Type *lhs = fill_type(ctx, type->comma.lhs);
        const struct Type *rhs = fill_type(ctx, type->comma.rhs);
        return Type_comma(ctx->globals.intern, lhs, rhs);
    }
    case TT_Var: {
        const struct TypeLL *t = DSet_find_root(&ctx->equivs, type);
        assert(t && "Idek");
        type = t->current.type;
        assert(type->tag != TT_Var && "it seems not all types are resolved!");
        return fill_type(ctx, type);
    }
    }
}

static const struct Type *destructure_binding(
    struct Context *ctx, struct Binding *binding, bool mut
) {
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
        const struct Type *lhs = destructure_binding(ctx, binding->comma.lhs, mut);
        const struct Type *rhs = destructure_binding(ctx, binding->comma.rhs, mut);
        return Type_product(ctx->globals.intern, lhs, rhs);
    }
    }
}

static const struct Type *unify(
    struct serene_Allocator alloc, struct DSet *dset, const struct Type *lhs,
    const struct Type *rhs
) {
    struct TypeLL *lroot = DSet_root(DSet_insert(alloc, dset, lhs));
    const struct Type *ltype = lroot->current.type;
    struct TypeLL *rroot = DSet_root(DSet_insert(alloc, dset, rhs));
    const struct Type *rtype = rroot->current.type;
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
        assert((ltype->tag == TT_Var || rtype->tag == TT_Var) && "type mismatch");
    }
    return DSet_join(lroot, rroot)->current.type;
}

static struct TypeLL *DSet_insert(
    struct serene_Allocator alloc, struct DSet *this, const struct Type *type
) {
    for (ll_iter(head, this->types)) {
        if (head->current.type == type)
            return head;
    }
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

static struct TypeLL *DSet_find_root(struct DSet *this, const struct Type *type) {
    for (ll_iter(head, this->types)) {
        if (Type_cmp(head->current.type, type) == 0)
            return DSet_root(head);
    }
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
