#include "./typer.h"
#include <stdbool.h>

static bool Type_equal(struct Type, struct Type);
static struct Type Binding_type(struct Binding);

struct DSet {
    struct TypeLL {
        struct {
            struct TypeLL* parent;
            struct Type type;
        } current;
        struct TypeLL* next;
    }* types;
};

static struct TypeLL* DSet_insert(struct Arena*, struct DSet*, struct Type);
static struct TypeLL* DSet_root(struct DSet*, struct TypeLL*);
static struct TypeLL* DSet_join(struct Arena*, struct DSet*, struct TypeLL*, struct TypeLL*);

struct Globals {
    struct GlobalsLL {
        struct {
            const char* name;
            struct Type type;
        } current;
        struct GlobalsLL* next;
    }* globals;
};

static void typecheck_func(struct Arena*, struct Globals, struct Function);

struct Context {
    struct LetsLL {
        struct {
            const char* name;
            bool mutable;
            struct Type type;
        } current;
        struct LetsLL* next;
    }* lets;
    struct DSet equivs;
    struct Type ret;
    int var_counter;
};

static struct Type typecheck_expr(struct Arena*, struct Context*, struct Globals, struct Expr);
static void typecheck_stmt(struct Arena*, struct Context*, struct Globals, struct Statement);
static void destructure_binding(struct Arena*, struct Context*, struct Binding);
static struct Type Context_new_typevar(struct Context*);

void typecheck(struct Arena* arena, struct Ast ast) {
    struct Globals globals = {0};
    for (struct FunctionsLL* f = ast.funcs; f; f = f->next) {
        struct GlobalsLL* tmp = arena_talloc(arena, struct Type);
        struct Type* ret = arena_talloc(arena, struct Type);
        struct Type* args = arena_talloc(arena, struct Type);
        assert(tmp && ret && args);
        
        *ret = f->current.ret;
        *args = Binding_type(f->current.args);
        tmp->current.type = (struct Type) {
            .tag = TT_Func,
            .func.ret = ret,
            .func.args = args,
        };
        tmp->current.name = f->current.name;
        tmp->next = globals.globals;
        globals.globals = tmp;
    }
    for (struct FunctionsLL* f = ast.funcs; f; f = f->next) {
        typecheck_func(arena, globals, f->current);
    }
    return;
}

static void typecheck_func(
    struct Arena* arena, struct Globals globals, struct Function func
) {
    struct Context ctx = {0};
    destructure_binding(arena, &ctx, func.args);
    ctx.ret = func.ret;
    typecheck_expr(arena, &ctx, globals, func.body);
}

static struct Type typecheck_expr(
    struct Arena* arena,
    struct Context* ctx,
    struct Globals globals,
    struct Expr expr
) {
    switch (expr.tag) {
    case ET_Unit: assert(false && "no unit types yet");
    case ET_If: assert(false && "no bool types yet");
    case ET_Loop: assert(false && "no unit types yet");
    case ET_Bareblock: {
        // TODO: we should probably also revert arena state
        // so as to free up the memory that is 'leaked' here
        struct LetsLL* head = ctx->lets;
        for (struct StatementsLL* s = expr.bareblock.stmts; s; s = s->next) {
            typecheck_stmt(arena, ctx, globals, s->current);
        }
        struct Type out =
            typecheck_expr(arena, ctx, globals, *expr.bareblock.tail);
        // end scope
        ctx->lets = head;
        return out;
    }
    case ET_Call: {
        struct Type f = typecheck_expr(arena, ctx, globals, *expr.call.name);
        struct Type a = typecheck_expr(arena, ctx, globals, *expr.call.args);
        // unify f and Type_func(in: a, out: new_tvar())
        // then return the principals return type
        // which we can access, as either a function type (f or Type_func(...)) becomes principal, or we run into an error
        assert(false && "TODO");
    }
    case ET_Recall: {
        for (struct LetsLL* head = ctx->lets; head; head = head->next) {
            // TODO: string interning property
            if (strings_equal(head->current.name, expr.lit.name)) {
                return head->current.type;
            }
        }
        assert(false && "no such name!");
    }
    case ET_NumberLit: assert(false && "no int types yet");
    case ET_StringLit: assert(false && "no string types yet");
    case ET_BoolLit: assert(false && "no bool types yet");
    case ET_Comma: assert(false && "no product types yet");
    }
}

static void typecheck_stmt(
    struct Arena* arena,
    struct Context* ctx,
    struct Globals globals,
    struct Statement stmt
) {
    assert(false && "TODO");
}

static void destructure_binding(
    struct Arena* arena, struct Context* ctx, struct Binding binding
) {
    struct LetsLL* tmp;
    
    switch (binding.tag) {
    case BT_Empty: return;
    case BT_Name:
        tmp = arena_talloc(arena, struct LetsLL);
        assert(tmp);
        tmp->current.name = binding.name.name;
        tmp->current.mutable = false;
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

static struct Type Context_new_typevar(struct Context* ctx) {
    int v = ctx->var_counter;
    ctx->var_counter++;
    return (struct Type) {
        .tag = TT_Var,
        .var.idx = v,
    };
}

static bool Type_equal(struct Type lhs, struct Type rhs) {
    if (lhs.tag != rhs.tag) return false;
    switch (lhs.tag) {
    case TT_Func:
        return Type_equal(*lhs.func.args, *rhs.func.args) &&
            Type_equal(*lhs.func.ret, *rhs.func.ret);
    case TT_Call:
        return Type_equal(*lhs.call.name, *rhs.call.name) &&
            Type_equal(*lhs.call.args, *rhs.call.args);
    case TT_Recall:
        return strings_equal(lhs.recall.name, rhs.recall.name);
    case TT_Comma:
        return Type_equal(*lhs.comma.lhs, *rhs.comma.lhs) &&
            Type_equal(*lhs.comma.rhs, *rhs.comma.rhs);
    case TT_Var:
        return lhs.var.idx == rhs.var.idx;
    }
}

static struct Type Binding_type(struct Binding this) {
    switch (this.tag) {
    case BT_Empty:
        assert(false && "trouble with string interning");
    case BT_Name:
        return *this.name.annot;
    }
}

static struct TypeLL* DSet_insert(
    struct Arena* arena, struct DSet* this, struct Type type
) {
    for (struct TypeLL* head = this->types; head; head = head->next) {
        if (Type_equal(head->current.type, type)) return head;
    }
    struct TypeLL* tmp = arena_alloc(arena, sizeof(struct TypeLL));
    tmp->next = this->types;
    tmp->current.parent = tmp;
    tmp->current.type = type;
    this->types = tmp;
    return this->types;
}

static struct TypeLL* DSet_root(
    struct DSet* this, struct TypeLL* type
) {
    while (type->current.parent != type) {
        struct TypeLL* tmp = type;
        type = type->current.parent;
        tmp->current.parent = tmp->current.parent->current.parent;
    }
    return type;
}

static struct TypeLL* DSet_join(
    struct Arena* arena,
    struct DSet* this,
    struct TypeLL* lhs,
    struct TypeLL* rhs
) {
    struct TypeLL* lhr = DSet_root(this, lhs);
    struct TypeLL* rhr = DSet_root(this, rhs);
    if (lhr->current.type.tag == TT_Var) {
        struct TypeLL* tmp = lhr;
        lhr = rhr;
        rhr = tmp;
    }
    rhr->current.parent = lhr;
    return lhr;
}

