#include "./codegen.h"
#include "./common_ll.h"
#include "./strings.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <llvm-c/Analysis.h>

struct Ctx {
    struct serene_Allocator alloc;
    LLVMModuleRef mod;
    struct FuncsLL {
        struct FuncsLL *next;
        struct tst_Function *f;
        LLVMValueRef fval;
        LLVMTypeRef ftype;
    } *funcs;
    LLVMTypeRef t_unit;
    LLVMValueRef v_unit;
};

static LLVMTypeRef lower_type(struct Ctx* ctx, struct tst_Type* type);
static void lower_function(struct Ctx *ctx, struct tst_Function *func, LLVMValueRef fvar, LLVMTypeRef ftype);

LLVMModuleRef lower(struct Tst *tst, struct serene_Allocator alloc) {
    struct Ctx ctx = {
        .alloc = alloc,
        .mod = LLVMModuleCreateWithName("mod"),
        .funcs = NULL,
        .t_unit = LLVMStructType(NULL, 0, false),
    };
    ctx.v_unit = LLVMConstNamedStruct(ctx.t_unit, NULL, 0);
    for (ll_iter(f, tst->funcs)) {
        LLVMTypeRef type = lower_type(&ctx, &f->current.type);
        LLVMValueRef val = LLVMAddFunction(ctx.mod, f->current.name, type);
        struct FuncsLL *tmp = serene_alloc(ctx.alloc, struct FuncsLL);
        assert(tmp && "OOM");
        tmp->next = ctx.funcs;
        tmp->f = &f->current;
        tmp->ftype = type;
        tmp->fval = val;
        ctx.funcs = tmp;
    }
    for (ll_iter(f, ctx.funcs)) {
        lower_function(&ctx, f->f, f->fval, f->ftype);
    }
    char *error = NULL;
    if (LLVMVerifyModule(ctx.mod, LLVMAbortProcessAction, &error)) {
        printf("error arose:\n%s\n", error);
        assert(false);
    }
    return ctx.mod;
}

static LLVMTypeRef lower_TTT_Star(struct Ctx *ctx, struct tst_TypeStar *type),
    lower_TTT_Func(struct Ctx *ctx, struct tst_TypeFunc *type);

static LLVMTypeRef lower_type(struct Ctx *ctx, struct tst_Type *type) {
    switch (type->tag) {
    case TTT_Unit: return ctx->t_unit;
    case TTT_Int: return LLVMInt64Type();
    case TTT_Bool: return LLVMInt1Type();
    case TTT_String: return LLVMPointerType(LLVMInt8Type(), 0);
    case TTT_Star: return lower_TTT_Star(ctx, type->star);
    case TTT_Func: return lower_TTT_Func(ctx, type->func);
    }
    assert(false && "shouldn't");
}

static LLVMTypeRef lower_TTT_Star(struct Ctx* ctx, struct tst_TypeStar* type) {
    LLVMTypeRef elems[] = {
        lower_type(ctx, &type->lhs),
        lower_type(ctx, &type->rhs)
    };
    return LLVMStructType(elems, 2, false);
}

static LLVMTypeRef lower_TTT_Func(struct Ctx* ctx, struct tst_TypeFunc* type) {
    LLVMTypeRef args[] = { lower_type(ctx, &type->args) };
    LLVMTypeRef ret = lower_type(ctx, &type->ret);
    return LLVMFunctionType(ret, args, 1, false);
}

struct FCtx {
    struct Ctx *ctx;
    LLVMBuilderRef b;
    LLVMValueRef f;
    LLVMValueRef v_ret;
    LLVMBasicBlockRef b_ret;
    struct LetsLL {
        struct LetsLL *next;
        const char *name;
        LLVMValueRef slot;
    } *lets;
    struct LoopsLL {
        struct LoopsLL *next;
        LLVMBasicBlockRef post;
        LLVMValueRef v_break;
    } *loops;
};

struct Control {
    enum { CT_Plain, CT_Break, CT_Return } tag;
    LLVMValueRef val;
};
struct Control Control_plain(LLVMValueRef v) {
    return (struct Control){ .tag = CT_Plain, .val = v };
}
struct Control Control_break(LLVMValueRef v) {
    return (struct Control){ .tag = CT_Break, .val = v };
}
struct Control Control_return(LLVMValueRef v) {
    return (struct Control){ .tag = CT_Return, .val = v };
}

static struct Control lower_expr(struct tst_Expr *, struct FCtx *);
static void lower_binding(struct tst_Binding *, struct FCtx *, LLVMValueRef);

static void lower_function(
    struct Ctx* ctx, struct tst_Function *func, LLVMValueRef fval, LLVMTypeRef ftype
) {
    struct FCtx fctx = {
        .ctx = ctx,
        .b = LLVMCreateBuilder(),
        .f = fval,
        .lets = NULL,
        .loops = NULL,
    };
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fctx.f, "entry");
    fctx.b_ret = LLVMAppendBasicBlock(fctx.f, "b_ret");

    LLVMTypeRef t_ret = LLVMGetReturnType(ftype);
    LLVMPositionBuilderAtEnd(fctx.b, entry);
    fctx.v_ret = LLVMBuildAlloca(fctx.b, t_ret, "v_ret");

    lower_binding(&func->args, &fctx, LLVMGetParam(fctx.f, 0));
    struct Control out = lower_expr(&func->body, &fctx);
    assert(
        out.tag != CT_Break && "breaks in plain bareblocks shouldn't happen"
    );
    if (out.tag == CT_Plain) {
        LLVMBuildStore(fctx.b, out.val, fctx.v_ret);
        LLVMBuildBr(fctx.b, fctx.b_ret);
    }
    LLVMPositionBuilderAtEnd(fctx.b, fctx.b_ret);
    LLVMValueRef v_ret = LLVMBuildLoad2(fctx.b, t_ret, fctx.v_ret, "*v_ret");
    LLVMBuildRet(fctx.b, v_ret);
    LLVMDisposeBuilder(fctx.b);
}

static struct Control lower_TET_BoolLit(struct FCtx* ctx, const char* lit),
    lower_TET_NumberLit(struct FCtx* ctx, const char* lit),
    lower_TET_StringLit(struct FCtx* ctx, const char* lit),
    lower_TET_If(struct FCtx* ctx, struct tst_ExprIf* expr),
    lower_TET_Loop(struct FCtx* ctx, struct tst_Expr* body, struct tst_Type *type),
    lower_TET_Bareblock(struct FCtx* ctx, struct tst_ExprsLL* body),
    lower_TET_Call(struct FCtx* ctx, struct tst_ExprCall* expr),
    lower_TET_Recall(struct FCtx* ctx, const char* lit),
    lower_TET_Comma(struct FCtx* ctx, struct tst_ExprComma *expr, struct tst_Type *type),
    lower_TST_Let(struct FCtx* ctx, struct tst_ExprLet* expr),
    lower_TST_Break(struct FCtx* ctx, struct tst_Expr* body),
    lower_TST_Return(struct FCtx* ctx, struct tst_Expr* body),
    lower_TST_Assign(struct FCtx* ctx, struct tst_ExprAssign* expr),
    lower_TST_Const(struct FCtx* ctx, struct tst_Expr* body);

static struct Control lower_expr(struct tst_Expr* expr, struct FCtx* ctx) {
#define Case(Tag, ...) \
    case Tag: return lower_##Tag(ctx, __VA_ARGS__);

    switch (expr->tag) {
        case TET_Unit: return Control_plain(ctx->ctx->v_unit);
            Case(TET_BoolLit, expr->lit);
            Case(TET_NumberLit, expr->lit);
            Case(TET_StringLit, expr->lit);
            Case(TET_If, expr->if_expr);
            Case(TET_Loop, expr->loop, &expr->type);
            Case(TET_Bareblock, expr->bareblock);
            Case(TET_Call, expr->call);
            Case(TET_Recall, expr->lit);
            Case(TET_Comma, expr->comma, &expr->type);
            Case(TST_Let, expr->let);
            Case(TST_Break, expr->break_stmt);
            Case(TST_Return, expr->return_stmt);
            Case(TST_Assign, expr->assign);
            Case(TST_Const, expr->const_stmt);
    }
#undef Case
    assert(false && "shouldn't");
}

static struct Control lower_TET_BoolLit(struct FCtx* ctx, const char* lit) {
    int b = 0;
    if (strings_equal(lit, "true")) b = 1;
    return Control_plain(LLVMConstInt(LLVMInt1Type(), b, false));
}

static struct Control lower_TET_NumberLit(struct FCtx* ctx, const char* lit) {
    unsigned long long n = atoi(lit);
    return Control_plain(LLVMConstInt(LLVMInt64Type(), n, false));
}

static struct Control lower_TET_StringLit(struct FCtx* ctx, const char* lit) {
    return Control_plain(LLVMConstString(lit, strings_strlen(lit), false));
}

static struct Control lower_TET_If(struct FCtx* ctx, struct tst_ExprIf* expr) {
    struct Control cond = lower_expr(&expr->cond, ctx);
    if (cond.tag == CT_Break || cond.tag == CT_Return) return cond;

    LLVMBasicBlockRef b_smash = LLVMAppendBasicBlock(ctx->f, "");
    LLVMBasicBlockRef b_pass = LLVMAppendBasicBlock(ctx->f, "");
    LLVMBasicBlockRef b_post = LLVMAppendBasicBlock(ctx->f, "");
    LLVMBuildCondBr(ctx->b, cond.val, b_smash, b_pass);

    LLVMPositionBuilderAtEnd(ctx->b, b_smash);
    struct Control v_smash = lower_expr(&expr->smash, ctx);
    if (v_smash.tag == CT_Plain) {
        LLVMBuildBr(ctx->b, b_post);
    }

    LLVMPositionBuilderAtEnd(ctx->b, b_pass);
    struct Control v_pass = lower_expr(&expr->pass, ctx);
    if (v_pass.tag == CT_Plain) {
        LLVMBuildBr(ctx->b, b_post);
    }

    if (v_pass.tag == CT_Break && v_smash.tag != CT_Plain) return v_pass;
    if (v_smash.tag == CT_Break && v_pass.tag != CT_Plain) return v_smash;
    if (v_pass.tag != CT_Plain && v_smash.tag != CT_Plain) return v_pass;
        
    LLVMPositionBuilderAtEnd(ctx->b, b_post);
    LLVMTypeRef t_phi;
    if (v_pass.tag == CT_Plain) t_phi = LLVMTypeOf(v_pass.val);
    else t_phi = LLVMTypeOf(v_smash.val);
    LLVMValueRef v_phi = LLVMBuildPhi(ctx->b, t_phi, "");
    LLVMValueRef v_incoming[2] = {0};
    LLVMBasicBlockRef b_incoming[2] = {0};
    int i_incoming = 0;
    if (v_smash.tag == CT_Plain) {
        v_incoming[i_incoming] = v_smash.val;
        b_incoming[i_incoming] = b_smash;
        i_incoming++;
    }
    if (v_pass.tag == CT_Plain) {
        v_incoming[i_incoming] = v_pass.val;
        b_incoming[i_incoming] = b_pass;
        i_incoming++;
    }
    LLVMAddIncoming(v_phi, v_incoming, b_incoming, i_incoming);
    return Control_plain(v_phi);
}

static struct Control lower_TET_Loop(struct FCtx* ctx, struct tst_Expr* body, struct tst_Type *type) {
    LLVMValueRef v_break = LLVMBuildAlloca(ctx->b, lower_type(ctx->ctx, type), "");
    LLVMBasicBlockRef b_loop = LLVMAppendBasicBlock(ctx->f, "");
    LLVMBasicBlockRef b_post = LLVMAppendBasicBlock(ctx->f, "");
    struct LoopsLL *tmp = serene_alloc(ctx->ctx->alloc, struct LoopsLL);
    assert(tmp && "OOM");
    tmp->next = ctx->loops;
    tmp->v_break = v_break;
    tmp->post = b_post;
    LLVMBuildBr(ctx->b, b_loop);
    LLVMPositionBuilderAtEnd(ctx->b, b_loop);
    struct Control v = lower_expr(body, ctx);
    if (v.tag == CT_Return) return v;
    if (v.tag == CT_Plain) {
        LLVMBuildBr(ctx->b, b_loop);
    }
    LLVMPositionBuilderAtEnd(ctx->b, b_post);
    ctx->loops = ctx->loops->next;
    return Control_plain(LLVMBuildLoad2(ctx->b, LLVMTypeOf(v_break), v_break, ""));
}

static struct Control lower_TET_Bareblock(struct FCtx* ctx, struct tst_ExprsLL* body) {
    LLVMValueRef out = ctx->ctx->v_unit;
    for (ll_iter(head, body)) {
        struct Control tmp = lower_expr(&head->current, ctx);
        if (tmp.tag == CT_Break || tmp.tag == CT_Return) return tmp;
        out = tmp.val;
    }
    return Control_plain(out);
}

static struct Control lower_TET_Call(struct FCtx* ctx, struct tst_ExprCall* expr) {
    struct Control name = lower_expr(&expr->name, ctx);
    if (name.tag == CT_Return || name.tag == CT_Break) return name;
    struct Control args = lower_expr(&expr->args, ctx);
    if (args.tag == CT_Return || args.tag == CT_Break) return args;
    LLVMValueRef res = LLVMBuildCall2(ctx->b, LLVMTypeOf(name.val), name.val, &args.val, 1, "");
    return Control_plain(res);
}

static struct Control lower_TET_Recall(struct FCtx* ctx, const char* lit) {
    for (ll_iter(head, ctx->lets)) {
        if (head->name == lit) {
            return Control_plain(LLVMBuildLoad2(ctx->b, LLVMTypeOf(head->slot), head->slot, head->name));
        }
    }
    for (ll_iter(head, ctx->ctx->funcs)) {
        if (head->f->name == lit) {
            return Control_plain(head->fval);
        }
    }
    assert(false && "no such name found, something in typer must've gone wrong!");
}

static struct Control lower_TET_Comma(struct FCtx* ctx, struct tst_ExprComma* expr, struct tst_Type *type) {
    LLVMTypeRef llvm_type = lower_type(ctx->ctx, type);
    LLVMValueRef comma = LLVMGetUndef(llvm_type);
    struct Control lhs = lower_expr(&expr->lhs, ctx);
    if (lhs.tag == CT_Break || lhs.tag == CT_Return) return lhs;
    struct Control rhs = lower_expr(&expr->rhs, ctx);
    if (rhs.tag == CT_Break || rhs.tag == CT_Return) return rhs;
    comma = LLVMBuildInsertValue(ctx->b, comma, lhs.val, 0, "");
    comma = LLVMBuildInsertValue(ctx->b, comma, rhs.val, 1, "");
    return Control_plain(comma);
}

static struct Control lower_TST_Let(struct FCtx* ctx, struct tst_ExprLet* expr) {
    struct Control v = lower_expr(&expr->init, ctx);
    if (v.tag == CT_Break || v.tag == CT_Return) return v;
    lower_binding(&expr->bind, ctx, v.val);
    return Control_plain(ctx->ctx->v_unit);
}

static struct Control lower_TST_Break(struct FCtx* ctx, struct tst_Expr* body) {
    struct Control v_break = lower_expr(body, ctx);
    if (v_break.tag == CT_Break || v_break.tag == CT_Return) return v_break;
    LLVMValueRef v_ptr = ctx->loops->v_break;
    LLVMBuildStore(ctx->b, v_break.val, v_ptr);
    LLVMBuildBr(ctx->b, ctx->loops->post);
    return Control_break(ctx->ctx->v_unit);
}

static struct Control lower_TST_Return(struct FCtx* ctx, struct tst_Expr* body) {
    struct Control v_ret = lower_expr(body, ctx);
    if (v_ret.tag == CT_Break || v_ret.tag == CT_Return) return v_ret;
    LLVMBuildStore(ctx->b, v_ret.val, ctx->v_ret);
    LLVMBuildBr(ctx->b, ctx->b_ret);
    return Control_return(ctx->ctx->v_unit);
}

static struct Control lower_TST_Assign(struct FCtx* ctx, struct tst_ExprAssign* expr) {
    for (ll_iter(head, ctx->lets)) {
        if (head->name == expr->name) {
            struct Control val = lower_expr(&expr->expr, ctx);
            if (val.tag == CT_Break || val.tag == CT_Return) return val;
            LLVMBuildStore(ctx->b, val.val, head->slot);
            return Control_plain(ctx->ctx->v_unit);
        }
    }
    assert(false && "no such name, typer must've fucky wuckied");
}

static struct Control lower_TST_Const(struct FCtx* ctx, struct tst_Expr* body) {
    lower_expr(body, ctx);
    return Control_plain(ctx->ctx->v_unit);
}

static void lower_binding(
    struct tst_Binding *binding, struct FCtx *ctx, LLVMValueRef val
) {
    switch (binding->tag) {
    case TBT_Empty: return;
    case TBT_Name: {
        LLVMValueRef loc = LLVMBuildAlloca(
            ctx->b, lower_type(ctx->ctx, &binding->name.type), ""
        );
        LLVMBuildStore(ctx->b, val, loc);
        struct LetsLL *tmp = serene_alloc(ctx->ctx->alloc, struct LetsLL);
        assert(tmp && "OOM");
        tmp->next = ctx->lets;
        tmp->name = binding->name.name;
        tmp->slot = loc;
        ctx->lets = tmp;
        return;
    }
    case TBT_Comma:
        assert(false && "TODO");
    }
}
