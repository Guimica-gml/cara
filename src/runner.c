#include "./runner.h"
#include "./common_ll.h"
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

static int runner_atoi(const char*);

enum ValueTag {
    VT_Unit = 0,
    VT_Int,
    VT_Bool,
    VT_String,
    VT_Comma,
    VT_Func,
};
struct Value {
    enum ValueTag tag;
    union {
        int v_int;
        bool v_bool;
        const char* v_str;
        struct {
            struct Value* lhs;
            struct Value* rhs;
        } v_comma;
        const struct Function* v_func;
    };
};

static void print_val(struct Value);
static struct Value Value_clone(struct Arena*, struct Value);

struct Control {
    enum { CT_Return, CT_Break, CT_Plain } tag;
    struct Value val;
};

static struct Control Control_return(struct Value);
static struct Control Control_break(struct Value);
static struct Control Control_plain(struct Value);

struct Globals {
    struct Symbols symbols;
    struct GlobalsLL {
        struct {
            const char* name;
            struct Value val;
        } current;
        struct GlobalsLL* next;
    }* globals;
};

static struct Value run_func(struct Arena*, struct Globals, struct Function, struct Value);

struct Context {
    struct LetsLL {
        struct {
            const char* name;
            struct Value val;
        } current;
        struct LetsLL* next;
    }* lets;
    struct Globals globals;
};

static void declare_binding(struct Arena*, struct Context*, struct Binding);
static void assign_binding(struct Context*, struct Binding, struct Value);
static struct Control run_expr(struct Arena*, struct Context*, struct Expr);
static struct Control run_stmt(struct Arena*, struct Context*, struct Statement);

void run(struct Arena* arena, struct Symbols symbols, struct Ast ast) {
    struct Globals globals = {0};
    globals.symbols = symbols;
    const struct Function* main_func = NULL;
    for (ll_iter(f, ast.funcs)) {
        struct GlobalsLL* tmp = arena_talloc(arena, struct GlobalsLL);
        assert(tmp);
        tmp->current.name = f->current.name;
        tmp->current.val = (struct Value) {
            .tag = VT_Func,
            .v_func = &f->current,
        };
        tmp->next = globals.globals;
        globals.globals = tmp;
        if (f->current.name == symbols.s_main) {
            main_func = &f->current;
        }
    }
    assert(main_func);
    print_val(run_func(arena, globals, *main_func, (struct Value) {0}));
}

static struct Value run_func(
    struct Arena* arena,
    struct Globals globals,
    struct Function func,
    struct Value args
) {
    struct Context ctx = {0};
    ctx.globals = globals;
    declare_binding(arena, &ctx, func.args);
    assign_binding(&ctx, func.args, args);
    return run_expr(arena, &ctx, func.body).val;
}

static struct Control run_expr(
    struct Arena* arena, struct Context* ctx, struct Expr expr
) {
    switch (expr.tag) {
    case ET_Unit: return Control_plain((struct Value) {0});
    case ET_If: {
        struct Control v = run_expr(arena, ctx, *expr.if_expr.cond);
        if (v.tag == CT_Return || v.tag == CT_Break) return v;
        assert(v.val.tag == VT_Bool);
        if (v.val.v_bool) {
            return run_expr(arena, ctx, *expr.if_expr.smash);
        } else {
            return run_expr(arena, ctx, *expr.if_expr.pass);
        }
    }
    case ET_Loop: while (true) {
        struct Control v = run_expr(arena, ctx, *expr.loop.block);
        if (v.tag == CT_Return) return v;
        if (v.tag == CT_Break) return Control_plain(v.val);
    }
    case ET_Bareblock: {
        struct LetsLL* head = ctx->lets;
        for (ll_iter(s, expr.bareblock.stmts)) {
            struct Control c = run_stmt(arena, ctx, s->current);
            if (c.tag == CT_Return || c.tag == CT_Break) return c;
        }
        struct Control out = run_expr(arena, ctx, *expr.bareblock.tail);
        ctx->lets = head;
        return out;
    };
    case ET_Call: {
        struct Control v = run_expr(arena, ctx, *expr.call.name);
        if (v.tag == CT_Return || v.tag == CT_Break) return v;
        assert(v.val.tag == VT_Func);
        struct Control a = run_expr(arena, ctx, *expr.call.args);
        if (a.tag == CT_Return || a.tag == CT_Break) return a;
        struct Value r = run_func(arena, ctx->globals, *v.val.v_func, a.val);
        return Control_plain(r);
    }
    case ET_Recall: {
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name != expr.lit.name) continue;
            return Control_plain(Value_clone(arena, head->current.val));
        }
        for (ll_iter(head, ctx->globals.globals)) {
            if (head->current.name != expr.lit.name) continue;
            return Control_plain(Value_clone(arena, head->current.val));
        }
        break;
    }
    case ET_NumberLit:
        return Control_plain((struct Value) {
            .tag = VT_Int,
            .v_int = runner_atoi(expr.lit.name),
        });
    case ET_StringLit:
        return Control_plain((struct Value) {
            .tag = VT_String,
            .v_str = expr.lit.name,
        });
    case ET_BoolLit:
        return Control_plain((struct Value) {
            .tag = VT_Bool,
            .v_bool = expr.lit.name == ctx->globals.symbols.s_true,
        });
    case ET_Comma: {
        struct Value* lhs = arena_talloc(arena, struct Value);
        struct Value* rhs = arena_talloc(arena, struct Value);
        assert(lhs && rhs);
        struct Control clhs = run_expr(arena, ctx, *expr.comma.lhs);
        if (clhs.tag == CT_Return || clhs.tag == CT_Break) return clhs;
        struct Control crhs = run_expr(arena, ctx, *expr.comma.rhs);
        if (crhs.tag == CT_Return || crhs.tag == CT_Break) return crhs;
        *lhs = clhs.val;
        *rhs = crhs.val;
        return Control_plain((struct Value) {
            .tag = VT_Comma,
            .v_comma.lhs = lhs,
            .v_comma.rhs = rhs,
        });
    }
    }
    
    assert(false && "shouldn't happen wtf");
}

static struct Control run_stmt(
    struct Arena* arena, struct Context* ctx, struct Statement stmt
) {
    switch (stmt.tag) {
    case ST_Mut: case ST_Let: {
        struct Control v = run_expr(arena, ctx, *stmt.let.init);
        if (v.tag == CT_Return || v.tag == CT_Break) return v;
        declare_binding(arena, ctx, stmt.let.bind);
        assign_binding(ctx, stmt.let.bind, v.val);
        return Control_plain((struct Value) {0});
    }
    case ST_Break: {
        struct Control v = run_expr(arena, ctx, *stmt.break_stmt.expr);
        if (v.tag == CT_Return) return v;
        return Control_break(v.val);
    }
    case ST_Return: {
        struct Control v = run_expr(arena, ctx, *stmt.return_stmt.expr);
        if (v.tag == CT_Break) return v;
        return Control_return(v.val);
    }
    case ST_Assign: {
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name != stmt.assign.name) continue;
            struct Control v = run_expr(arena, ctx, *stmt.assign.expr);
            if (v.tag == CT_Return || v.tag == CT_Break) return v;
            head->current.val = v.val;
            return Control_plain((struct Value) {0});
        }
        break;
    };
    case ST_Const: {
        struct Control v = run_expr(arena, ctx, *stmt.const_stmt.expr);
        if (v.tag == CT_Break || v.tag == CT_Return) return v;
        return Control_plain((struct Value) {0});
    }
    }

    assert(false && "shouldn't happen");
}

static void declare_binding(
    struct Arena* arena, struct Context* ctx, struct Binding binding
) {
    switch (binding.tag) {
    case BT_Empty: return;
    case BT_Name: {
        struct LetsLL* tmp = arena_talloc(arena, struct LetsLL);
        assert(tmp);
        tmp->current.name = binding.name.name;
        tmp->current.val = (struct Value) {0};
        tmp->next = ctx->lets;
        ctx->lets = tmp;
        return;
    }
    }
}

static void assign_binding(
    struct Context* ctx, struct Binding binding, struct Value value
) {
    switch (binding.tag) {
    case BT_Empty: return;
    case BT_Name: {
        for (ll_iter(head, ctx->lets)) {
            if (head->current.name != binding.name.name) continue;
            head->current.val = value;
            return;
        }
    }
    }
}

static struct Control Control_return(struct Value val) {
    return (struct Control) {
        .tag = CT_Return,
        .val = val,
    };
}

static struct Control Control_break(struct Value val) {
    return (struct Control) {
        .tag = CT_Break,
        .val = val,
    };
}

static struct Control Control_plain(struct Value val) {
    return (struct Control) {
        .tag = CT_Plain,
        .val = val,
    };
}

static struct Value Value_clone(struct Arena* arena, struct Value val) {
    switch (val.tag) {
    case VT_Unit: case VT_Int: case VT_Bool:
    case VT_String: case VT_Func: return val;
    case VT_Comma: {
        struct Value* lhs = arena_talloc(arena, struct Value);
        struct Value* rhs = arena_talloc(arena, struct Value);
        assert(lhs && rhs);
        *lhs = Value_clone(arena, *val.v_comma.lhs);
        *rhs = Value_clone(arena, *val.v_comma.rhs);
        return (struct Value) {
            .tag = VT_Comma,
            .v_comma.lhs = lhs,
            .v_comma.rhs = rhs,
        };
    }
    }
    
    assert(false && "gcc is trying to gaslight me again");
}

static void print_val(struct Value val) {
    switch (val.tag) {
    case VT_Unit:
        printf("()");
        break;
    case VT_Int:
        printf("%d", val.v_int);
        break;
    case VT_Bool:
        if (val.v_bool) {
            printf("true");
        } else {
            printf("false");
        }
        break;
    case VT_String:
        printf("%s", val.v_str);
        break;
    case VT_Comma:
        print_val(*val.v_comma.lhs);
        printf(", ");
        print_val(*val.v_comma.rhs);
        break;
    case VT_Func:
        printf("func@%s", val.v_func->name);
        break;
    }
}

static int runner_atoi(const char* str) {
    // this should probably be put into the lexer instead but eh
    (void) str;
    assert(false && "TODO");
}
