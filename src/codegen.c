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

LLVMTypeRef lower_type(struct tst_Type *, struct Ctx *);
void lower_function(struct tst_Function *, LLVMValueRef, LLVMTypeRef, struct Ctx *);

LLVMModuleRef lower(struct Tst *tst, struct serene_Allocator alloc) {
    struct Ctx ctx = {
        .alloc = alloc,
        .mod = LLVMModuleCreateWithName("mod"),
        .funcs = NULL,
        .t_unit = LLVMStructType(NULL, 0, false),
    };
    ctx.v_unit = LLVMConstNamedStruct(ctx.t_unit, NULL, 0);
    for (ll_iter(f, tst->funcs)) {
        LLVMTypeRef type = lower_type(&f->current.type, &ctx);
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
        lower_function(f->f, f->fval, f->ftype, &ctx);
    }
    char *error = NULL;
    if (LLVMVerifyModule(ctx.mod, LLVMAbortProcessAction, &error)) {
        printf("error arose:\n%s\n", error);
        assert(false);
    }
    return ctx.mod;
}

LLVMTypeRef lower_type(struct tst_Type *type, struct Ctx *ctx) {
    switch (type->tag) {
    case TTT_Unit: return ctx->t_unit;
    case TTT_Int: return LLVMInt64Type();
    case TTT_Bool: return LLVMInt1Type();
    case TTT_String: return LLVMPointerType(LLVMInt8Type(), 0);
    case TTT_Star: {
        LLVMTypeRef elems[] = {
            lower_type(type->star.lhs, ctx),
            lower_type(type->star.rhs, ctx)
        };
        return LLVMStructType(elems, 2, false);
    }
    case TTT_Func: {
        LLVMTypeRef args[] = { lower_type(type->func.args, ctx) };
        LLVMTypeRef ret = lower_type(type->func.ret, ctx);
        return LLVMFunctionType(ret, args, 1, false);
    }
    }
    assert(false && "shouldn't");
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

struct Control lower_expr(struct tst_Expr *, struct FCtx *);
void lower_binding(struct tst_Binding *, struct FCtx *, LLVMValueRef);

void lower_function(
    struct tst_Function *func, LLVMValueRef fval, LLVMTypeRef ftype, struct Ctx *ctx
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

struct Control lower_expr(struct tst_Expr *expr, struct FCtx *ctx) {
    switch (expr->tag) {
    case TET_Unit: return Control_plain(ctx->ctx->v_unit);
    case TET_BoolLit: {
        int b = 0;
        if (strings_equal(expr->lit, "true")) b = 1;
        return Control_plain(LLVMConstInt(LLVMInt1Type(), b, false));
    }
    case TET_NumberLit: {
        unsigned long long n = atoi(expr->lit);
        return Control_plain(LLVMConstInt(LLVMInt64Type(), n, false));
    }
    case TET_StringLit:
        return Control_plain(
            LLVMConstString(expr->lit, strings_strlen(expr->lit), false)
        );

    case TET_If: {
        struct Control cond = lower_expr(expr->if_expr.cond, ctx);
        if (cond.tag == CT_Break || cond.tag == CT_Return) return cond;

        LLVMBasicBlockRef b_smash = LLVMAppendBasicBlock(ctx->f, "");
        LLVMBasicBlockRef b_pass = LLVMAppendBasicBlock(ctx->f, "");
        LLVMBasicBlockRef b_post = LLVMAppendBasicBlock(ctx->f, "");
        LLVMBuildCondBr(ctx->b, cond.val, b_smash, b_pass);

        LLVMPositionBuilderAtEnd(ctx->b, b_smash);
        struct Control v_smash = lower_expr(expr->if_expr.smash, ctx);
        if (v_smash.tag == CT_Plain) {
            LLVMBuildBr(ctx->b, b_post);
        }

        LLVMPositionBuilderAtEnd(ctx->b, b_pass);
        struct Control v_pass = lower_expr(expr->if_expr.pass, ctx);
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
    case TET_Loop: {
        LLVMValueRef v_break = LLVMBuildAlloca(ctx->b, lower_type(&expr->type, ctx->ctx), "");
        LLVMBasicBlockRef b_loop = LLVMAppendBasicBlock(ctx->f, "");
        LLVMBasicBlockRef b_post = LLVMAppendBasicBlock(ctx->f, "");
        struct LoopsLL *tmp = serene_alloc(ctx->ctx->alloc, struct LoopsLL);
        assert(tmp && "OOM");
        tmp->next = ctx->loops;
        tmp->v_break = v_break;
        tmp->post = b_post;
        LLVMBuildBr(ctx->b, b_loop);
        LLVMPositionBuilderAtEnd(ctx->b, b_loop);
        struct Control v = lower_expr(expr->loop, ctx);
        if (v.tag == CT_Return) return v;
        if (v.tag == CT_Plain) {
            LLVMBuildBr(ctx->b, b_loop);
        }
        LLVMPositionBuilderAtEnd(ctx->b, b_post);
        ctx->loops = ctx->loops->next;
        return Control_plain(LLVMBuildLoad2(ctx->b, LLVMTypeOf(v_break), v_break, ""));
    }
    case TET_Bareblock: {
        LLVMValueRef out = ctx->ctx->v_unit;
        for (ll_iter(head, expr->bareblock)) {
            struct Control tmp = lower_expr(&head->current, ctx);
            if (tmp.tag == CT_Break || tmp.tag == CT_Return) return tmp;
            out = tmp.val;
        }
        return Control_plain(out);
    }
    case TET_Call: {
        struct Control name = lower_expr(expr->call.name, ctx);
        if (name.tag == CT_Return || name.tag == CT_Break) return name;
        struct Control args = lower_expr(expr->call.args, ctx);
        if (args.tag == CT_Return || args.tag == CT_Break) return args;
        LLVMValueRef res = LLVMBuildCall2(ctx->b, LLVMTypeOf(name.val), name.val, &args.val, 1, "");
        return Control_plain(res);
    }
    case TET_Recall: {
        for (ll_iter(head, ctx->lets)) {
            if (head->name == expr->lit) {
                return Control_plain(LLVMBuildLoad2(ctx->b, LLVMTypeOf(head->slot), head->slot, head->name));
            }
        }
        for (ll_iter(head, ctx->ctx->funcs)) {
            if (head->f->name == expr->lit) {
                return Control_plain(head->fval);
            }
        }
        assert(false && "no such name found, something in typer must've gone wrong!");
    }
    case TET_Comma: {
        LLVMTypeRef type = lower_type(&expr->type, ctx->ctx);
        LLVMValueRef comma = LLVMGetUndef(type);
        struct Control lhs = lower_expr(expr->comma.lhs, ctx);
        if (lhs.tag == CT_Break || lhs.tag == CT_Return) return lhs;
        struct Control rhs = lower_expr(expr->comma.rhs, ctx);
        if (rhs.tag == CT_Break || rhs.tag == CT_Return) return rhs;
        comma = LLVMBuildInsertValue(ctx->b, comma, lhs.val, 0, "");
        comma = LLVMBuildInsertValue(ctx->b, comma, rhs.val, 1, "");
        return Control_plain(comma);
    }

    case TST_Let: {
        struct Control v = lower_expr(expr->let.init, ctx);
        if (v.tag == CT_Break || v.tag == CT_Return) return v;
        lower_binding(&expr->let.bind, ctx, v.val);
        return Control_plain(ctx->ctx->v_unit);
    }
    case TST_Break: {
        struct Control v_break = lower_expr(expr->break_stmt, ctx);
        if (v_break.tag == CT_Break || v_break.tag == CT_Return) return v_break;
        LLVMValueRef v_ptr = ctx->loops->v_break;
        LLVMBuildStore(ctx->b, v_break.val, v_ptr);
        LLVMBuildBr(ctx->b, ctx->loops->post);
        return Control_break(ctx->ctx->v_unit);
    }
    case TST_Return: {
        struct Control v_ret = lower_expr(expr->return_stmt, ctx);
        if (v_ret.tag == CT_Break || v_ret.tag == CT_Return) return v_ret;
        LLVMBuildStore(ctx->b, v_ret.val, ctx->v_ret);
        LLVMBuildBr(ctx->b, ctx->b_ret);
        return Control_return(ctx->ctx->v_unit);
    }
    case TST_Assign: {
        for (ll_iter(head, ctx->lets)) {
            if (head->name == expr->assign.name) {
                struct Control val = lower_expr(expr->assign.expr, ctx);
                if (val.tag == CT_Break || val.tag == CT_Return) return val;
                LLVMBuildStore(ctx->b, val.val, head->slot);
                return Control_plain(ctx->ctx->v_unit);
            }
        }
        assert(false && "no such name, typer must've fucky wuckied");
    }
    case TST_Const:
        lower_expr(expr->const_stmt, ctx);
        return Control_plain(ctx->ctx->v_unit);
    }
    assert(false && "shouldn't");
}

void lower_binding(
    struct tst_Binding *binding, struct FCtx *ctx, LLVMValueRef val
) {
    switch (binding->tag) {
    case TBT_Empty: return;
    case TBT_Name: {
        LLVMValueRef loc = LLVMBuildAlloca(
            ctx->b, lower_type(&binding->name.type, ctx->ctx), ""
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
