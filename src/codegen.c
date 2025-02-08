#include "./codegen.h"

#include "./common_ll.h"
#include "./strings.h"

#include <assert.h>
#include <llvm-c/Analysis.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct Ctx {
    struct serene_Allocator alloc;
    LLVMModuleRef mod;
    struct FuncsLL {
        struct FuncsLL* next;
        struct tst_Function* f;
        LLVMValueRef fval;
        LLVMTypeRef ftype;
    }* funcs;
    LLVMTypeRef t_unit;
    LLVMValueRef v_unit;
};

static LLVMTypeRef lower_type(struct Ctx* ctx, struct tst_Type* type);
static void lower_function(struct Ctx* ctx, struct tst_Function* func, LLVMValueRef fvar, LLVMTypeRef ftype);

LLVMModuleRef lower(struct Tst* tst, struct serene_Allocator alloc) {
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
        struct FuncsLL* tmp = serene_alloc(ctx.alloc, struct FuncsLL);
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
    LLVMDumpModule(ctx.mod);
    char* error = NULL;
    if (LLVMVerifyModule(ctx.mod, LLVMAbortProcessAction, &error)) {
        printf("error arose:\n%s\n", error);
        assert(false);
    }
    return ctx.mod;
}

static LLVMTypeRef lower_TTT_Star(struct Ctx* ctx, struct tst_TypeStar* type),
    lower_TTT_Func(struct Ctx* ctx, struct tst_TypeFunc* type);

static LLVMTypeRef lower_type(struct Ctx* ctx, struct tst_Type* type) {
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
    int count = 0;
    for (ll_iter(head, type)) count++;

    LLVMTypeRef* elems = serene_nalloc(ctx->alloc, count, LLVMTypeRef);
    assert(elems && "OOM");
    int idx = 0;
    for (ll_iter(head, type), idx++) {
        elems[idx] = lower_type(ctx, &head->current);
    }

    return LLVMStructType(elems, count, false);
}

static LLVMTypeRef lower_TTT_Func(struct Ctx* ctx, struct tst_TypeFunc* type) {
    LLVMTypeRef args[] = {lower_type(ctx, &type->args)};
    LLVMTypeRef ret = lower_type(ctx, &type->ret);
    return LLVMFunctionType(ret, args, 1, false);
}

struct FCtx {
    struct Ctx* ctx;
    LLVMBuilderRef b;
    LLVMValueRef f;
    LLVMValueRef v_ret;
    LLVMBasicBlockRef b_ret;
    struct LetsLL {
        struct LetsLL* next;
        const char* name;
        LLVMValueRef slot;
        LLVMTypeRef type;
    }* lets;
    struct LoopsLL {
        struct LoopsLL* next;
        LLVMBasicBlockRef post;
        LLVMValueRef v_break;
        LLVMTypeRef type;
    }* loops;
};

struct Control {
    enum { CT_Plain,
           CT_Break,
           CT_Return } tag;
    LLVMValueRef val;
};
struct Control Control_plain(LLVMValueRef v) {
    return (struct Control){.tag = CT_Plain, .val = v};
}
struct Control Control_break(LLVMValueRef v) {
    return (struct Control){.tag = CT_Break, .val = v};
}
struct Control Control_return(LLVMValueRef v) {
    return (struct Control){.tag = CT_Return, .val = v};
}

static struct Control lower_expr(struct tst_Expr*, struct FCtx*);
static void lower_binding(struct tst_Binding*, struct FCtx*, LLVMValueRef);

static void lower_function(
    struct Ctx* ctx,
    struct tst_Function* func,
    LLVMValueRef fval,
    LLVMTypeRef ftype
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
    lower_TET_Loop(struct FCtx* ctx, struct tst_Expr* body, struct tst_Type* type),
    lower_TET_Bareblock(struct FCtx* ctx, struct tst_ExprsLL* body),
    lower_TET_Call(struct FCtx* ctx, struct tst_ExprCall* expr),
    lower_TET_Recall(struct FCtx* ctx, const char* lit),
    lower_TET_Tuple(struct FCtx* ctx, struct tst_ExprTuple* expr, struct tst_Type* type),
    lower_TET_Builtin(struct FCtx* ctx, enum tst_ExprBuiltin built),
    lower_TET_Cast(struct FCtx *ctx, struct tst_ExprCast* cast),
    lower_TST_Let(struct FCtx* ctx, struct tst_ExprLet* expr),
    lower_TST_Break(struct FCtx* ctx, struct tst_Expr* body),
    lower_TST_Return(struct FCtx* ctx, struct tst_Expr* body),
    lower_TST_Assign(struct FCtx* ctx, struct tst_ExprAssign* expr),
    lower_TST_Const(struct FCtx* ctx, struct tst_Expr* body);

static struct Control lower_expr(struct tst_Expr* expr, struct FCtx* ctx) {
#define Case(Tag, ...) \
    case Tag: return lower_##Tag(ctx, __VA_ARGS__);

    switch (expr->tag) {
        Case(TET_BoolLit, expr->lit);
        Case(TET_NumberLit, expr->lit);
        Case(TET_StringLit, expr->lit);
        Case(TET_If, expr->if_expr);
        Case(TET_Loop, expr->loop, &expr->type);
        Case(TET_Bareblock, expr->bareblock);
        Case(TET_Call, expr->call);
        Case(TET_Recall, expr->lit);
        Case(TET_Tuple, expr->tuple, &expr->type);
        Case(TET_Builtin, expr->builtin);
        Case(TET_Cast, expr->cast);
        Case(TST_Let, expr->let);
        Case(TST_Break, expr->break_stmt);
        Case(TST_Return, expr->return_stmt);
        Case(TST_Assign, expr->assign);
        Case(TST_Const, expr->const_stmt);
        case TET_Unit: return Control_plain(ctx->ctx->v_unit);
    }
#undef Case
    assert(false && "shouldn't");
}

static struct Control lower_TET_BoolLit(struct FCtx* ctx, const char* lit) {
    (void) ctx;
    int b = 0;
    if (strings_equal(lit, "true")) b = 1;
    return Control_plain(LLVMConstInt(LLVMInt1Type(), b, false));
}

static struct Control lower_TET_NumberLit(struct FCtx* ctx, const char* lit) {
    (void) ctx;
    unsigned long long n = atoi(lit);
    return Control_plain(LLVMConstInt(LLVMInt64Type(), n, false));
}

static struct Control lower_TET_StringLit(struct FCtx* ctx, const char* lit) {
    // creates a local string array
    /* (void) ctx; */
    /* return Control_plain(LLVMConstString(lit, strings_strlen(lit), false)); */
    return Control_plain(LLVMBuildGlobalString(ctx->b, lit, ""));
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

static struct Control lower_TET_Loop(struct FCtx* ctx, struct tst_Expr* body, struct tst_Type* type) {
    LLVMTypeRef t_break = lower_type(ctx->ctx, type);
    LLVMValueRef v_break = LLVMBuildAlloca(ctx->b, t_break, "");
    LLVMBasicBlockRef b_loop = LLVMAppendBasicBlock(ctx->f, "");
    LLVMBasicBlockRef b_post = LLVMAppendBasicBlock(ctx->f, "");
    struct LoopsLL* tmp = serene_alloc(ctx->ctx->alloc, struct LoopsLL);
    assert(tmp && "OOM");
    tmp->next = ctx->loops;
    tmp->v_break = v_break;
    tmp->type = t_break;
    tmp->post = b_post;
    ctx->loops = tmp;
    LLVMBuildBr(ctx->b, b_loop);
    LLVMPositionBuilderAtEnd(ctx->b, b_loop);
    struct Control v = lower_expr(body, ctx);
    if (v.tag == CT_Return) return v;
    if (v.tag == CT_Plain) {
        LLVMBuildBr(ctx->b, b_loop);
    }
    LLVMPositionBuilderAtEnd(ctx->b, b_post);
    ctx->loops = ctx->loops->next;
    return Control_plain(LLVMBuildLoad2(ctx->b, t_break, v_break, ""));
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

static struct Control lower_builtin_call(
    struct FCtx* ctx,
    enum tst_ExprBuiltin tag,
    struct tst_Expr args
) {
    LLVMValueRef v_args[7] = {0};
    if (args.tag == TET_Tuple) {
        int idx = 0;
        for (ll_iter(head, args.tuple), idx++) {
            assert(idx < 7 && "shouldn't happen anyway, but for safety");
            struct Control current = lower_expr(&head->current, ctx);
            if (current.tag == CT_Break || current.tag == CT_Return) return current;
            v_args[idx] = current.val;
        }
    } else {
        struct Control arg = lower_expr(&args, ctx);
        if (arg.tag == CT_Break || arg.tag == CT_Return) return arg;
        v_args[0] = arg.val;
    }
    LLVMValueRef out;
    switch (tag) {
        case EB_badd: out = LLVMBuildAdd(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bsub: out = LLVMBuildSub(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bmul: out = LLVMBuildMul(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bdiv: out = LLVMBuildUDiv(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bmod: out = LLVMBuildURem(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_band: out = LLVMBuildAnd(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bor: out = LLVMBuildOr(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bxor: out = LLVMBuildXor(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bshl: out = LLVMBuildShl(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bshr: out = LLVMBuildLShr(ctx->b, v_args[0], v_args[1], ""); break;
        case EB_bcmpEQ: out = LLVMBuildICmp(ctx->b, LLVMIntEQ, v_args[0], v_args[1], ""); break;
        case EB_bcmpNE: out = LLVMBuildICmp(ctx->b, LLVMIntNE, v_args[0], v_args[1], ""); break;
        case EB_bcmpGT: out = LLVMBuildICmp(ctx->b, LLVMIntUGT, v_args[0], v_args[1], ""); break;
        case EB_bcmpLT: out = LLVMBuildICmp(ctx->b, LLVMIntULT, v_args[0], v_args[1], ""); break;
        case EB_bcmpGE: out = LLVMBuildICmp(ctx->b, LLVMIntUGE, v_args[0], v_args[1], ""); break;
        case EB_bcmpLE: out = LLVMBuildICmp(ctx->b, LLVMIntULE, v_args[0], v_args[1], ""); break;
        case EB_bneg: out = LLVMBuildNeg(ctx->b, v_args[0], ""); break;
        case EB_bnot: out = LLVMBuildNot(ctx->b, v_args[0], ""); break;
        case EB_syscall: {
            char string[7] = "syscall";
            char regs[42] = "=r,{rax},{rdi},{rsi},{rdx},{r8},{r9},{r10}";
            LLVMTypeRef i64 = LLVMInt64Type();
            LLVMTypeRef params[7] = { i64, i64, i64, i64, i64, i64, i64 };
            LLVMTypeRef type = LLVMFunctionType(i64, params, 7, false);
            LLVMValueRef v_asm = LLVMGetInlineAsm(type, string, 7, regs, 42, true, false, LLVMInlineAsmDialectATT, false);
            out = LLVMBuildCall2(ctx->b, type, v_asm, v_args, 7, "");
            break;
        }
    }
    return Control_plain(out);
}

static struct Control lower_TET_Call(struct FCtx* ctx, struct tst_ExprCall* expr) {
    if (expr->name.tag == TET_Builtin)
        return lower_builtin_call(ctx, expr->name.builtin, expr->args);

    struct Control name = lower_expr(&expr->name, ctx);
    if (name.tag == CT_Return || name.tag == CT_Break) return name;
    struct Control args = lower_expr(&expr->args, ctx);
    if (args.tag == CT_Return || args.tag == CT_Break) return args;
    LLVMValueRef res = LLVMBuildCall2(ctx->b, LLVMGlobalGetValueType(name.val), name.val, &args.val, 1, "");
    return Control_plain(res);
}

static struct Control lower_TET_Recall(struct FCtx* ctx, const char* lit) {
    for (ll_iter(head, ctx->lets)) {
        if (head->name == lit) {
            return Control_plain(LLVMBuildLoad2(ctx->b, head->type, head->slot, head->name));
        }
    }
    for (ll_iter(head, ctx->ctx->funcs)) {
        if (head->f->name == lit) {
            return Control_plain(head->fval);
        }
    }
    assert(false && "no such name found, something in typer must've gone wrong!");
}

static struct Control lower_TET_Tuple(struct FCtx* ctx, struct tst_ExprTuple* expr, struct tst_Type* type) {
    LLVMTypeRef llvm_type = lower_type(ctx->ctx, type);
    LLVMValueRef comma = LLVMGetUndef(llvm_type);
    int idx = 0;
    for (ll_iter(head, expr), idx++) {
        struct Control current = lower_expr(&head->current, ctx);
        if (current.tag == CT_Break || current.tag == CT_Return) return current;
        comma = LLVMBuildInsertValue(ctx->b, comma, current.val, idx, "");
    }
    return Control_plain(comma);
}

static struct Control lower_TET_Builtin(struct FCtx* ctx, enum tst_ExprBuiltin built) {
    (void) ctx;
    (void) built;
    assert(false && "should not be called");
}

static struct Control lower_TET_Cast(struct FCtx* ctx, struct tst_ExprCast* cast) {
    struct Control v = lower_expr(&cast->expr, ctx);
    if (v.tag == CT_Break || v.tag == CT_Return) return v;
    LLVMTypeRef t = lower_type(ctx->ctx, &cast->type);
    return Control_plain(LLVMBuildPtrToInt(ctx->b, v.val, t, ""));
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
    struct Control v = lower_expr(body, ctx);
    if (v.tag == CT_Break || v.tag == CT_Return) return v;
    return Control_plain(ctx->ctx->v_unit);
}

static void lower_binding(
    struct tst_Binding* binding,
    struct FCtx* ctx,
    LLVMValueRef val
) {
    switch (binding->tag) {
        case TBT_Empty: return;
        case TBT_Name: {
            LLVMTypeRef type = lower_type(ctx->ctx, &binding->name.type);
            LLVMValueRef loc = LLVMBuildAlloca(ctx->b, type, "");
            LLVMBuildStore(ctx->b, val, loc);
            struct LetsLL* tmp = serene_alloc(ctx->ctx->alloc, struct LetsLL);
            assert(tmp && "OOM");
            tmp->next = ctx->lets;
            tmp->name = binding->name.name;
            tmp->slot = loc;
            tmp->type = type;
            ctx->lets = tmp;
            return;
        }
        case TBT_Tuple: {
            int idx = 0;
            for (ll_iter(head, binding->tuple), idx++) {
                LLVMValueRef current = LLVMBuildExtractValue(ctx->b, val, idx, "");
                lower_binding(&head->current, ctx, current);
            }
            return;
        }
    }
}
