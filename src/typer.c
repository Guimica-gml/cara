#include "./typer.h"
#include "./common_ll.h"
#include <assert.h>
#include <stdbool.h>

static bool Type_equal(struct Type, struct Type);

struct DSet {
    struct TypeLL {
        struct {
            struct TypeLL *parent;
            struct Type type;
        } current;
        struct TypeLL *next;
    } *types;
};

static struct TypeLL *
DSet_insert(struct serene_Allocator, struct DSet *, struct Type);
static struct TypeLL *DSet_root(struct TypeLL *);
static struct TypeLL *DSet_join(struct TypeLL *, struct TypeLL *);

struct Globals {
    struct Symbols symbols;
    struct GlobalsLL {
        struct {
            const char *name;
            struct Type type;
        } current;
        struct GlobalsLL *next;
    } *globals;
};

static struct Type
unify(struct serene_Allocator, struct DSet *, struct Type, struct Type);
static struct tst_Function
    typecheck_func(struct serene_Allocator, struct Globals, struct Function);

struct Context {
    struct LetsLL {
        struct {
            const char *name;
            bool mutable;
            struct Type type;
        } current;
        struct LetsLL *next;
    } *lets;
    struct LoopsLL {
        struct Type current;
        struct LoopsLL *next;
    } *loops;
    struct DSet equivs;
    struct Type ret;
    struct Globals globals;
};

static struct Type
typecheck_expr(struct serene_Allocator, struct Context *, struct Expr);
static struct tst_Expr
convert_expr(struct serene_Allocator, struct Context *, struct Expr);
static void
destructure_binding(struct serene_Allocator, struct Context *, struct Binding, bool, struct Type *);
static struct tst_Binding
convert_binding(struct serene_Allocator, struct Context *, struct Binding);
static struct tst_Type
convert_type(struct serene_Allocator, struct Context *, struct Type);

struct Tst typecheck(
    struct serene_Allocator alloc, struct Symbols symbols, struct Ast ast
) {
    struct Globals globals = {0};
    globals.symbols = symbols;

    for (ll_iter(f, ast.funcs)) {
        struct GlobalsLL *tmp = serene_alloc(alloc, struct GlobalsLL);
        struct Type *ret = serene_alloc(alloc, struct Type);
        struct Type *args = serene_alloc(alloc, struct Type);
        assert(tmp && ret && args);

        *ret = f->current.ret;
        *args = Binding_to_type(symbols, f->current.args);
        tmp->current.type = (struct Type){
            .tag = TT_Func,
            .func.ret = ret,
            .func.args = args,
        };
        tmp->current.name = f->current.name;
        tmp->next = globals.globals;
        globals.globals = tmp;
    }

    struct Tst out = {0};
    struct tst_FunctionsLL *last = NULL;
    for (ll_iter(f, ast.funcs)) {
        struct tst_FunctionsLL *tmp =
            serene_alloc(alloc, struct tst_FunctionsLL);
        assert(tmp && "OOM");
        if (!last)
            out.funcs = tmp;
        else
            last->next = tmp;
        last = tmp;

        last->current = typecheck_func(alloc, globals, f->current);
    }

    return out;
}

static struct tst_Function typecheck_func(
    struct serene_Allocator alloc, struct Globals globals, struct Function func
) {
    struct Context ctx = {0};
    ctx.globals = globals;
    destructure_binding(alloc, &ctx, func.args, false, NULL);
    ctx.ret = func.ret;
    typecheck_expr(alloc, &ctx, func.body);

    struct tst_Expr body = convert_expr(alloc, &ctx, func.body);
    struct tst_Binding args = convert_binding(alloc, &ctx, func.args);
    struct tst_Type type;
    for (ll_iter(f, globals.globals)) {
        if (f->current.name == func.name) {
            type = convert_type(alloc, &ctx, f->current.type);
            break;
        }
    }
    return (struct tst_Function){
        .name = func.name,
        .type = type,
        .args = args,
        .body = body,
    };
}

static struct Type typecheck_expr(
    struct serene_Allocator alloc, struct Context *ctx, struct Expr expr
) {
    switch (expr.tag) {
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
    case ET_Unit:
        return expr.type;

    case ET_If: {
        struct Type cond = typecheck_expr(alloc, ctx, *expr.if_expr.cond);
        unify(alloc, &ctx->equivs, cond, Type_bool(ctx->globals.symbols));
        struct Type smash = typecheck_expr(alloc, ctx, *expr.if_expr.smash);
        struct Type pass = typecheck_expr(alloc, ctx, *expr.if_expr.pass);
        return unify(alloc, &ctx->equivs, smash, pass);
    }
    case ET_Loop: {
        struct Type out = expr.type;
        struct LoopsLL *head = serene_alloc(alloc, struct LoopsLL);
        head->current = out;
        head->next = ctx->loops;
        ctx->loops = head;
        typecheck_expr(alloc, ctx, *expr.loop);
        ctx->loops = head->next;
        return out;
    }
    case ET_Bareblock: {
        // TODO: we should probably also revert arena state
        // so as to free up the memory that is 'leaked' here
        // that is however complicated, as the arena is used
        // for many things beyond ctx->lets
        struct Type out = Type_unit(ctx->globals.symbols);
        struct LetsLL *head = ctx->lets;
        for (ll_iter(s, expr.bareblock)) {
            out = typecheck_expr(alloc, ctx, s->current);
        }
        ctx->lets = head;
        return out;
    }
    case ET_Call: {
        struct Type f;
        struct Type *a = serene_alloc(alloc, struct Type);
        struct Type *r = serene_alloc(alloc, struct Type);
        assert(a && r);
        f = typecheck_expr(alloc, ctx, *expr.call.name);
        *a = typecheck_expr(alloc, ctx, *expr.call.args);
        *r = expr.type;
        struct Type fa =
            (struct Type){.tag = TT_Func, .func.args = a, .func.ret = r};
        struct Type p = unify(alloc, &ctx->equivs, f, fa);
        assert(p.tag == TT_Func);
        return *p.func.ret;
    }
    case ET_Recall: {
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name == expr.lit) {
                return unify(
                    alloc, &ctx->equivs, expr.type, head->current.type
                );
            }
        }
        for (ll_iter(head, ctx->globals.globals)) {
            if (head->current.name == expr.lit) {
                return unify(
                    alloc, &ctx->equivs, expr.type, head->current.type
                );
            }
        }
        assert(false && "no such name!");
    }
    case ET_Comma: {
        typecheck_expr(alloc, ctx, *expr.comma.lhs);
        typecheck_expr(alloc, ctx, *expr.comma.rhs);
        return expr.type;
    }

    case ST_Let: {
        struct Type it = typecheck_expr(alloc, ctx, *expr.let.init);
        destructure_binding(alloc, ctx, expr.let.bind, false, &it);
        return expr.type;
    }
    case ST_Mut: {
        struct Type it = typecheck_expr(alloc, ctx, *expr.let.init);
        destructure_binding(alloc, ctx, expr.let.bind, true, &it);
        return expr.type;
    }
    case ST_Break: {
        struct Type brk = typecheck_expr(alloc, ctx, *expr.break_stmt);
        assert(ctx->loops);
        unify(alloc, &ctx->equivs, brk, ctx->loops->current);
        return expr.type;
    }
    case ST_Return: {
        struct Type ret = typecheck_expr(alloc, ctx, *expr.return_stmt);
        unify(alloc, &ctx->equivs, ret, ctx->ret);
        return expr.type;
    }
    case ST_Assign:
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name == expr.assign.name) {
                assert(
                    head->current.mutable && "tried modifying an immutable var!"
                );
                struct Type ass = typecheck_expr(alloc, ctx, *expr.assign.expr);
                return unify(alloc, &ctx->equivs, ass, head->current.type);
            }
        }
        assert(false && "no such variable declared!");
    case ST_Const:
        typecheck_expr(alloc, ctx, *expr.const_stmt);
        return expr.type;
    }
    assert(false && "gcc complains about control reaching here??");
}

static void destructure_binding(
    struct serene_Allocator alloc, struct Context *ctx, struct Binding binding,
    bool mut, struct Type *type
) {
    switch (binding.tag) {
    case BT_Empty:
        return;
    case BT_Name: {
        struct LetsLL *tmp = serene_alloc(alloc, struct LetsLL);
        assert(tmp);
        tmp->current.name = binding.name.name;
        tmp->current.mutable = mut;
        tmp->current.type = binding.name.annot;
        tmp->next = ctx->lets;
        ctx->lets = tmp;
        // we want to avoid destructuring the type
        // in the case this function becomes recursive
        // pass NULL to further calls
        if (type)
            unify(alloc, &ctx->equivs, *type, tmp->current.type);
        return;
    }
    }
}

static bool Type_equal(struct Type lhs, struct Type rhs) {
    if (lhs.tag != rhs.tag)
        return false;
    switch (lhs.tag) {
    case TT_Func:
        return Type_equal(*lhs.func.args, *rhs.func.args) &&
               Type_equal(*lhs.func.ret, *rhs.func.ret);
    case TT_Call:
        return Type_equal(*lhs.call.name, *rhs.call.name) &&
               Type_equal(*lhs.call.args, *rhs.call.args);
    case TT_Comma:
        return Type_equal(*lhs.comma.lhs, *rhs.comma.lhs) &&
               Type_equal(*lhs.comma.rhs, *rhs.comma.rhs);
    case TT_Recall:
        return lhs.recall == rhs.recall;
    case TT_Var:
        return lhs.var == rhs.var;
    }
    assert(false && "gcc complains about control reaching here??");
}

static struct Type unify(
    struct serene_Allocator alloc, struct DSet *dset, struct Type lhs,
    struct Type rhs
) {
    struct TypeLL *lroot = DSet_root(DSet_insert(alloc, dset, lhs));
    struct Type ltype = lroot->current.type;
    struct TypeLL *rroot = DSet_root(DSet_insert(alloc, dset, rhs));
    struct Type rtype = rroot->current.type;
    if (ltype.tag == rtype.tag) {
        switch (ltype.tag) {
        case TT_Call:
            unify(alloc, dset, *ltype.call.name, *rtype.call.name);
            unify(alloc, dset, *ltype.call.args, *rtype.call.args);
            return lroot->current.type;
        case TT_Func:
            unify(alloc, dset, *ltype.func.args, *rtype.func.args);
            unify(alloc, dset, *ltype.func.ret, *rtype.func.ret);
            return ltype;
        case TT_Comma:
            unify(alloc, dset, *ltype.comma.lhs, *rtype.comma.lhs);
            unify(alloc, dset, *ltype.comma.rhs, *rtype.comma.rhs);
            return ltype;
        case TT_Recall:
            assert(ltype.recall == rtype.recall && "type mismatch");
            return ltype;
        case TT_Var:
            break;
        }
    } else {
        assert((ltype.tag == TT_Var || rtype.tag == TT_Var) && "type mismatch");
    }
    return DSet_join(lroot, rroot)->current.type;
}

static struct TypeLL *DSet_insert(
    struct serene_Allocator alloc, struct DSet *this, struct Type type
) {
    for (ll_iter(head, this->types)) {
        if (Type_equal(head->current.type, type))
            return head;
    }
    struct TypeLL *tmp = serene_alloc(alloc, struct TypeLL);
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

static struct TypeLL *DSet_join(struct TypeLL *lhs, struct TypeLL *rhs) {
    struct TypeLL *lhr = DSet_root(lhs);
    struct TypeLL *rhr = DSet_root(rhs);
    if (lhr->current.type.tag == TT_Var) {
        struct TypeLL *tmp = lhr;
        lhr = rhr;
        rhr = tmp;
    }
    rhr->current.parent = lhr;
    return lhr;
}

static struct tst_Expr convert_expr(
    struct serene_Allocator alloc, struct Context *ctx, struct Expr expr
) {
    struct tst_Type type = convert_type(alloc, ctx, expr.type);
    switch (expr.tag) {
    case ET_NumberLit:
        return (struct tst_Expr){
            .tag = TET_NumberLit,
            .type = type,
            .lit = expr.lit,
        };
    case ET_StringLit:
        return (struct tst_Expr){
            .tag = TET_StringLit,
            .type = type,
            .lit = expr.lit,
        };
    case ET_BoolLit:
        return (struct tst_Expr){
            .tag = TET_BoolLit,
            .type = type,
            .lit = expr.lit,
        };
    case ET_Unit:
        return (struct tst_Expr){
            .tag = TET_Unit,
            .type = type,
        };

    case ET_If: {
        struct tst_Expr *cond = serene_alloc(alloc, struct tst_Expr);
        struct tst_Expr *smash = serene_alloc(alloc, struct tst_Expr);
        struct tst_Expr *pass = serene_alloc(alloc, struct tst_Expr);
        assert(cond && smash && pass && "OOM");
        *cond = convert_expr(alloc, ctx, *expr.if_expr.cond);
        *smash = convert_expr(alloc, ctx, *expr.if_expr.smash);
        *pass = convert_expr(alloc, ctx, *expr.if_expr.pass);
        return (struct tst_Expr){
            .tag = TET_If,
            .type = type,
            .if_expr.cond = cond,
            .if_expr.smash = smash,
            .if_expr.pass = pass,
        };
    }
    case ET_Loop: {
        struct tst_Expr *body = serene_alloc(alloc, struct tst_Expr);
        assert(body && "OOM");
        *body = convert_expr(alloc, ctx, *expr.loop);
        return (struct tst_Expr){
            .tag = TET_Loop,
            .type = type,
            .loop = body,
        };
    }
    case ET_Bareblock: {
        struct tst_ExprsLL *body = NULL;
        struct tst_ExprsLL *last = NULL;
        for (ll_iter(head, expr.bareblock)) {
            struct tst_ExprsLL *tmp = serene_alloc(alloc, struct tst_ExprsLL);
            assert(tmp && "OOM");
            if (!last)
                body = tmp;
            else
                last->next = tmp;
            last = tmp;

            last->current = convert_expr(alloc, ctx, head->current);
        }
        return (struct tst_Expr){
            .tag = TET_Bareblock,
            .type = type,
            .bareblock = body,
        };
    }
    case ET_Call: {
        struct tst_Expr *name = serene_alloc(alloc, struct tst_Expr);
        struct tst_Expr *args = serene_alloc(alloc, struct tst_Expr);
        assert(name && args && "OOM");
        *name = convert_expr(alloc, ctx, *expr.call.name);
        *args = convert_expr(alloc, ctx, *expr.call.args);
        return (struct tst_Expr){
            .tag = TET_Call,
            .type = type,
            .call.name = name,
            .call.args = args,
        };
    }
    case ET_Recall:
        return (struct tst_Expr
        ){.tag = TET_Recall, .type = type, .lit = expr.lit};
    case ET_Comma: {
        struct tst_Expr *lhs = serene_alloc(alloc, struct tst_Expr);
        struct tst_Expr *rhs = serene_alloc(alloc, struct tst_Expr);
        assert(lhs && rhs && "OOM");
        *lhs = convert_expr(alloc, ctx, *expr.comma.lhs);
        *rhs = convert_expr(alloc, ctx, *expr.comma.rhs);
        return (struct tst_Expr){
            .tag = TET_Comma,
            .type = type,
            .comma.lhs = lhs,
            .comma.rhs = rhs,
        };
    }

        // reify them, as we already checked no let's are mutated
    case ST_Mut:
    case ST_Let: {
        struct tst_Binding bind = convert_binding(alloc, ctx, expr.let.bind);
        struct tst_Expr *init = serene_alloc(alloc, struct tst_Expr);
        assert(init && "OOM");
        *init = convert_expr(alloc, ctx, *expr.let.init);
        return (struct tst_Expr){
            .tag = TST_Let,
            .type = type,
            .let.bind = bind,
            .let.init = init,
        };
    }
    case ST_Break: {
        struct tst_Expr *e = serene_alloc(alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(alloc, ctx, *expr.break_stmt);
        return (struct tst_Expr){
            .tag = TST_Break,
            .type = type,
            .break_stmt = e,
        };
    }
    case ST_Return: {
        struct tst_Expr *e = serene_alloc(alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(alloc, ctx, *expr.return_stmt);
        return (struct tst_Expr){
            .tag = TST_Return,
            .type = type,
            .return_stmt = e,
        };
    }
    case ST_Assign: {
        struct tst_Expr *e = serene_alloc(alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(alloc, ctx, *expr.assign.expr);
        return (struct tst_Expr){
            .tag = TST_Assign,
            .type = type,
            .assign.name = expr.assign.name,
            .assign.expr = e,
        };
    }
    case ST_Const: {
        struct tst_Expr *e = serene_alloc(alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(alloc, ctx, *expr.const_stmt);
        return (struct tst_Expr){
            .tag = TST_Const,
            .type = type,
            .const_stmt = e,
        };
    }
    }
    assert(false && "gcc complains about control reaching here??");
}

static struct tst_Binding convert_binding(
    struct serene_Allocator alloc, struct Context *ctx, struct Binding binding
) {
    switch (binding.tag) {
    case BT_Empty:
        return (struct tst_Binding){
            .tag = TBT_Empty,
        };
    case BT_Name:
        return (struct tst_Binding){
            .tag = TBT_Name,
            .name.name = binding.name.name,
            .name.type = convert_type(alloc, ctx, binding.name.annot),
        };
    }
}

static struct tst_Type convert_type(
    struct serene_Allocator alloc, struct Context *ctx, struct Type type
) {
    struct tst_Type *a = serene_alloc(alloc, struct tst_Type);
    struct tst_Type *b = serene_alloc(alloc, struct tst_Type);
    struct tst_Type out = {0};
    switch (type.tag) {
    case TT_Func:
        *a = convert_type(alloc, ctx, *type.func.args);
        *b = convert_type(alloc, ctx, *type.func.ret);
        out = (struct tst_Type){
            .tag = TTT_Func,
            .func.args = a,
            .func.ret = b,
        };
        break;
    case TT_Call:
        *a = convert_type(alloc, ctx, *type.call.name);
        *b = convert_type(alloc, ctx, *type.call.args);
        out = (struct tst_Type){
            .tag = TTT_Call,
            .call.name = a,
            .call.args = b,
        };
        break;
    case TT_Comma:
        *a = convert_type(alloc, ctx, *type.comma.lhs);
        *b = convert_type(alloc, ctx, *type.comma.rhs);
        out = (struct tst_Type){
            .tag = TTT_Comma,
            .comma.lhs = a,
            .comma.rhs = b,
        };
        break;
    case TT_Recall: {
        struct Symbols syms = ctx->globals.symbols;
        if (syms.s_unit == type.recall) {
            out.tag = TTT_Unit;
        } else if (syms.s_int == type.recall) {
            out.tag = TTT_Int;
        } else if (syms.s_bool == type.recall) {
            out.tag = TTT_Bool;
        } else if (syms.s_string == type.recall) {
            out.tag = TTT_String;
        } else if (syms.s_star == type.recall) {
            out.tag = TTT_Star;
        } else {
            assert(false && "TODO");
        }
        break;
    }
    case TT_Var: {
        struct TypeLL *ll = DSet_insert(alloc, &ctx->equivs, type);
        ll = DSet_root(ll);
        assert(
            ll->current.type.tag != TT_Var &&
            "not all types were resolved it seems"
        );
        return convert_type(alloc, ctx, ll->current.type);
    }
    }
    return out;
}
