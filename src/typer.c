#include "./typer.h"
#include "./common_ll.h"
#include <assert.h>
#include <stdbool.h>

static bool Type_equal(struct Type, struct Type);
static struct Type Binding_type(struct Symbols, struct Binding);

struct DSet {
    struct TypeLL {
        struct {
            struct TypeLL *parent;
            struct Type type;
        } current;
        struct TypeLL *next;
    } *types;
};

static struct TypeLL *DSet_insert(struct Arena *, struct DSet *, struct Type);
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
unify(struct Arena *, struct DSet *, struct Type, struct Type);
static void typecheck_func(struct Arena *, struct Globals, struct Function);

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
    int var_counter;
    struct Globals globals;
};

static struct Type
typecheck_expr(struct Arena *, struct Context *, struct Expr);
static void typecheck_stmt(struct Arena *, struct Context *, struct Statement);
static void
destructure_binding(struct Arena *, struct Context *, struct Binding, bool);
static struct Type Context_new_typevar(struct Context *);

void typecheck(struct Arena *arena, struct Symbols symbols, struct Ast ast) {
    struct Globals globals = {0};
    globals.symbols = symbols;

    // for (struct FunctionsLL* f = ast.funcs; f; f = f->next) {
    for (ll_iter(f, ast.funcs)) {
        struct GlobalsLL *tmp = arena_talloc(arena, struct Type);
        struct Type *ret = arena_talloc(arena, struct Type);
        struct Type *args = arena_talloc(arena, struct Type);
        assert(tmp && ret && args);

        *ret = f->current.ret;
        *args = Binding_type(symbols, f->current.args);
        tmp->current.type = (struct Type){
            .tag = TT_Func,
            .func.ret = ret,
            .func.args = args,
        };
        tmp->current.name = f->current.name;
        tmp->next = globals.globals;
        globals.globals = tmp;
    }

    // for (struct FunctionsLL* f = ast.funcs; f; f = f->next) {
    for (ll_iter(f, ast.funcs)) {
        typecheck_func(arena, globals, f->current);
    }

    return;
}

static void typecheck_func(
    struct Arena *arena, struct Globals globals, struct Function func
) {
    struct Context ctx = {0};
    ctx.globals = globals;
    destructure_binding(arena, &ctx, func.args, false);
    ctx.ret = func.ret;
    typecheck_expr(arena, &ctx, func.body);
}

static struct Type
typecheck_expr(struct Arena *arena, struct Context *ctx, struct Expr expr) {
    switch (expr.tag) {
    case ET_Unit:
        return Type_unit(ctx->globals.symbols);
    case ET_If: {
        struct Type cond = typecheck_expr(arena, ctx, *expr.if_expr.cond);
        unify(arena, &ctx->equivs, cond, Type_bool(ctx->globals.symbols));
        struct Type smash = typecheck_expr(arena, ctx, *expr.if_expr.smash);
        struct Type pass = typecheck_expr(arena, ctx, *expr.if_expr.pass);
        return unify(arena, &ctx->equivs, smash, pass);
    }
    case ET_Loop: {
        struct Type out = Context_new_typevar(ctx);
        struct LoopsLL *head = arena_talloc(arena, struct LoopsLL);
        head->current = out;
        head->next = ctx->loops;
        ctx->loops = head;
        typecheck_expr(arena, ctx, *expr.loop.block);
        ctx->loops = head->next;
        return out;
    }
    case ET_Bareblock: {
        // TODO: we should probably also revert arena state
        // so as to free up the memory that is 'leaked' here
        // that is however complicated, as the arena is used
        // for many things beyond ctx->lets
        struct LetsLL *head = ctx->lets;
        for (ll_iter(s, expr.bareblock.stmts)) {
            typecheck_stmt(arena, ctx, s->current);
        }
        struct Type out = typecheck_expr(arena, ctx, *expr.bareblock.tail);
        // end scope
        ctx->lets = head;
        return out;
    }
    case ET_Call: {
        struct Type f;
        struct Type *a = arena_talloc(arena, struct Type);
        struct Type *r = arena_talloc(arena, struct Type);
        assert(a && r);
        f = typecheck_expr(arena, ctx, *expr.call.name);
        *a = typecheck_expr(arena, ctx, *expr.call.args);
        *r = Context_new_typevar(ctx);
        struct Type fa =
            (struct Type){.tag = TT_Func, .func.args = a, .func.ret = r};
        struct Type p = unify(arena, &ctx->equivs, f, fa);
        assert(p.tag == TT_Func);
        return *p.func.ret;
    }
    case ET_Recall: {
        // for (struct LetsLL* head = ctx->lets; head; head = head->next) {
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name == expr.lit.name) {
                return head->current.type;
            }
        }
        // for (struct GlobalsLL* head = ctx->globals.globals; head; head =
        // head->next) {
        for (ll_iter(head, ctx->globals.globals)) {
            if (head->current.name == expr.lit.name) {
                return head->current.type;
            }
        }
        assert(false && "no such name!");
    }
    case ET_NumberLit:
        return Type_int(ctx->globals.symbols);
    case ET_StringLit:
        return Type_string(ctx->globals.symbols);
    case ET_BoolLit:
        return Type_bool(ctx->globals.symbols);
    case ET_Comma: {
        struct Type *lhs = arena_talloc(arena, struct Type);
        struct Type *rhs = arena_talloc(arena, struct Type);
        struct Type *args = arena_talloc(arena, struct Type);
        struct Type *prod = arena_talloc(arena, struct Type);
        assert(lhs && rhs && args && prod);

        *lhs = typecheck_expr(arena, ctx, *expr.comma.lhs);
        *rhs = typecheck_expr(arena, ctx, *expr.comma.rhs);
        *args = (struct Type){
            .tag = TT_Comma,
            .comma.lhs = lhs,
            .comma.rhs = rhs,
        };
        *prod = (struct Type){
            .tag = TT_Recall,
            .recall.name = ctx->globals.symbols.s_star,
        };

        return (struct Type){
            .tag = TT_Call,
            .call.name = prod,
            .call.args = args,
        };
    }
    }
    assert(false && "gcc complains about control reaching here??");
}

static void typecheck_stmt(
    struct Arena *arena, struct Context *ctx, struct Statement stmt
) {
    switch (stmt.tag) {
    case ST_Let:
        // TODO: make bindings respect initialiezr type
        destructure_binding(arena, ctx, stmt.let.bind, false);
        typecheck_expr(arena, ctx, *stmt.let.init);
        break;
    case ST_Mut:
        destructure_binding(arena, ctx, stmt.let.bind, true);
        typecheck_expr(arena, ctx, *stmt.let.init);
        break;
    case ST_Break: {
        struct Type brk = typecheck_expr(arena, ctx, *stmt.break_stmt.expr);
        assert(ctx->loops);
        unify(arena, &ctx->equivs, brk, ctx->loops->current);
        break;
    }
    case ST_Return: {
        struct Type ret = typecheck_expr(arena, ctx, *stmt.return_stmt.expr);
        unify(arena, &ctx->equivs, ret, ctx->ret);
        break;
    }
    case ST_Assign:
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name == stmt.assign.name) {
                assert(
                    head->current.mutable && "tried modifying an immutable var!"
                );
            }
        }
        assert(false && "no such variable declared!");
    case ST_Const:
        typecheck_expr(arena, ctx, *stmt.const_stmt.expr);
        break;
    }
}

static void destructure_binding(
    struct Arena *arena, struct Context *ctx, struct Binding binding, bool mut
) {
    switch (binding.tag) {
    case BT_Empty:
        return;
    case BT_Name: {
        struct LetsLL *tmp = arena_talloc(arena, struct LetsLL);
        assert(tmp);
        tmp->current.name = binding.name.name;
        tmp->current.mutable = mut;
        if (binding.name.annot) {
            tmp->current.type = *binding.name.annot;
        } else {
            tmp->current.type = Context_new_typevar(ctx);
        }
        tmp->next = ctx->lets;
        ctx->lets = tmp;
        return;
    }
    }
}

static struct Type Context_new_typevar(struct Context *ctx) {
    int v = ctx->var_counter;
    ctx->var_counter++;
    return (struct Type){
        .tag = TT_Var,
        .var.idx = v,
    };
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
    case TT_Recall:
        return lhs.recall.name == rhs.recall.name;
    case TT_Comma:
        return Type_equal(*lhs.comma.lhs, *rhs.comma.lhs) &&
               Type_equal(*lhs.comma.rhs, *rhs.comma.rhs);
    case TT_Var:
        return lhs.var.idx == rhs.var.idx;
    }
    assert(false && "gcc complains about control reaching here??");
}

static struct Type Binding_type(struct Symbols symbols, struct Binding this) {
    switch (this.tag) {
    case BT_Empty:
        return (struct Type){
            .tag = TT_Recall,
            .recall.name = symbols.s_unit,
        };
    case BT_Name:
        return *this.name.annot;
    }
    assert(false && "gcc complains about control reaching here??");
}

static struct Type unify(
    struct Arena *arena, struct DSet *dset, struct Type lhs, struct Type rhs
) {
    struct TypeLL *lroot = DSet_root(DSet_insert(arena, dset, lhs));
    struct Type ltype = lroot->current.type;
    struct TypeLL *rroot = DSet_root(DSet_insert(arena, dset, rhs));
    struct Type rtype = rroot->current.type;
    if (ltype.tag == rtype.tag) {
        switch (ltype.tag) {
        case TT_Call:
            unify(arena, dset, *ltype.call.name, *rtype.call.name);
            unify(arena, dset, *ltype.call.args, *rtype.call.args);
            return lroot->current.type;
        case TT_Func:
            unify(arena, dset, *ltype.func.args, *rtype.func.args);
            unify(arena, dset, *ltype.func.ret, *rtype.func.ret);
            return ltype;
        case TT_Comma:
            unify(arena, dset, *ltype.comma.lhs, *rtype.comma.lhs);
            unify(arena, dset, *ltype.comma.rhs, *rtype.comma.rhs);
            return ltype;
        case TT_Recall:
            assert(ltype.recall.name == rtype.recall.name && "type mismatch");
            return ltype;
        case TT_Var:
            break;
        }
    } else {
        assert((ltype.tag == TT_Var || rtype.tag == TT_Var) && "type mismatch");
    }
    return DSet_join(lroot, rroot)->current.type;
}

static struct TypeLL *
DSet_insert(struct Arena *arena, struct DSet *this, struct Type type) {
    // for (struct TypeLL* head = this->types; head; head = head->next) {
    for (ll_iter(head, this->types)) {
        if (Type_equal(head->current.type, type))
            return head;
    }
    struct TypeLL *tmp = arena_alloc(arena, sizeof(struct TypeLL));
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
