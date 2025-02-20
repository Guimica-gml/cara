#include "preimport.h"
#include "common_ll.h"
#include "commons.h"
#include <assert.h>
#include <stdio.h>

void PPData_print(void* _data) {
    struct PPData* data = _data;
    printf("PPD{ tokens * %zu, ops * %zu, ... }", data->toks.len, data->ops.len);
}

static struct PPImport resolve_import(struct PIImport* import, struct MTree* root) {
    struct MTree* mod = root;
    struct String decl = {0};
    for (ll_iter(part, import)) {
        struct MTree* tmp = MTree_index(mod, part->part);
        if (!part->next && !tmp) {
            decl = part->part;
            break;
        }
        assert(tmp && "tried importing from a nonexistent module");
        mod = tmp;
    }
    return (struct PPImport){mod, decl};
}

static void preimport_row(struct serene_Trea* alloc, struct MTree* row) {
    for (size_t i = 0; i < row->subs_count; i++) {
        struct MTree* head = &row->subs[i];
        struct PIData* data = head->data;
        if (!data) continue;
        struct PPData* new = serene_trealloc(alloc, struct PPData);
        assert(new && "OOM"), ZERO(*new);
        new->closure_status = CS_TODO;
        new->toks = data->toks;
        new->ops = data->ops;
        for (ll_iter(import, data->imports)) {
            struct PPImports* tmp = serene_trealloc(alloc, struct PPImports);
            assert(tmp && "OOM"), ZERO(*tmp);
            tmp->current = resolve_import(&import->current, row);
            tmp->next = new->imports;
            new->imports = tmp;
        }
        // Trea should handle freeing PIData
        head->data = new;
    }
}
static void closure(struct serene_Trea*, struct PPData*);

static struct Opdecls* get_closure(struct serene_Trea* alloc, struct MTree* mod) {
    struct PPData* data = mod->data;
    if (!data) return NULL;
    switch (data->closure_status) {
        case CS_TODO:
            closure(alloc, data);
            assert(data->closure_status == CS_DONE);
            return &data->ops;
        case CS_WIP: assert(false && "Import cycle!!!!");
        case CS_DONE: return &data->ops;
        default: assert(false && "unreachable");
    }
}

static void closure(struct serene_Trea* alloc, struct PPData* data) {
    if (!data) return;
    data->closure_status = CS_WIP;
    for (ll_iter(head, data->imports)) {
        struct Opdecls* closure = get_closure(alloc, head->current.mod);
        if (!closure) continue;
        for (size_t i = 0; i < closure->len; i++) {
            assert(Opdecls_push(&data->ops, closure->buf[i]));
        }
    }
    data->closure_status = CS_DONE;
}

struct Ctx {
    struct serene_Trea* alloc;
};

static void preimport_row_ptr(void* _ctx, struct MTree* mods) {
    struct Ctx* ctx = _ctx;
    preimport_row(ctx->alloc, mods);
}

static void* closure_ptr(void* _ctx, void* data) {
    struct Ctx* ctx = _ctx;
    closure(ctx->alloc, data);
    return data;
}

static void cleanup(void* _data) {
    (void)_data;
}

struct MTree* preimport(struct serene_Trea* alloc, struct MTree* mods) {
    struct Ctx ctx = {alloc};
    MTree_map_whole(mods, preimport_row_ptr, &ctx);
    MTree_map(mods, cleanup, closure_ptr, &ctx);
    return mods;
}
