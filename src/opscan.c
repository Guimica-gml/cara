#include "opscan.h"
#include "lexer.h"
#include "opdeclvec.h"
#include "tokenvec.h"
#include "commons.h"

#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>

void PIData_print(void* _data) {
    struct PIData* data = _data;
    printf("PID{ tokens * %zu, ops * %zu, ... }", data->toks.len, data->ops.len);
}

struct StringLL {
    struct StringLL* next;
    struct String current;
};

static struct String cat_strings(
    struct serene_Trea* scratch,
    struct StringLL* list,
    size_t cum_len
);

static void* scan_mod(
    struct serene_Trea* alloc,
    struct serene_Trea* scratch,
    struct Intern* intern,
    struct String* source
) {
    struct PIData* data = serene_trealloc(alloc, struct PIData);
    assert(data && "OOM"), ZERO(*data);
    struct Lexer lexer = {
        .rest = *source,
        .token = {0},
    };
#define SKIP t = Lexer_next(&lexer)
#define PUSH(T) assert(Tokenvec_push(&data->toks, T));
    struct Token t = Lexer_next(&lexer);
        while (t.kind != TK_EOF) {
            switch (t.kind) {
            case TK_String: {
                struct StringLL* list = serene_trealloc(scratch, struct StringLL);
                assert(list && "OOM");
                list->next = NULL;
                list->current = t.spelling;
                struct StringLL* last = list;
                size_t cum_len = t.spelling.len;

                for (SKIP; t.kind == TK_String; SKIP) {
                    struct StringLL* tmp = serene_trealloc(scratch, struct StringLL);
                    assert(tmp && "OOM");
                    *tmp = (typeof(*tmp)){0};
                    tmp->current = t.spelling;
                    last->next = tmp;
                    last = tmp;
                    cum_len += t.spelling.len;
                }

                struct String whole = cat_strings(scratch, list, cum_len);
                struct String spelling = Intern_insert(intern, whole);
                PUSH(((struct Token){.kind = TK_String, .spelling = spelling, .number = 0}));
                break;
            }
            case TK_Comment: {
                SKIP;
                break;
            }

            case TK_Operator: {
                SKIP;
                assert(t.kind == TK_OpenParen), SKIP;
                int lbp;
                if (t.kind == TK_Number) {
                    lbp = t.number;
                } else {
                    lbp = -1;
                    assert(t.spelling.str[0] == '_');
                    assert(t.spelling.len == 1);
                }
                SKIP;
                assert(t.kind == TK_Comma), SKIP;
                struct String name = Intern_insert(intern, t.spelling);
                assert(t.kind == TK_Name), SKIP;
                assert(t.kind == TK_Comma), SKIP;
                int rbp;
                if (t.kind == TK_Number) {
                    rbp = t.number;
                } else {
                    rbp = -1;
                    assert(t.spelling.str[0] == '_');
                    assert(t.spelling.len == 1);
                }
                SKIP;
                assert(t.kind == TK_CloseParen), SKIP;

                assert(Opdecls_push(&data->ops, (struct Opdecl){.token = name, .lbp = lbp, .rbp = rbp}));
                break;
            }

            case TK_Import: {
                SKIP;

                assert(t.kind == TK_Name);
                t.spelling = Intern_insert(intern, t.spelling);
                struct PIImport head = {
                    .next = NULL,
                    .part = t.spelling,
                };
                struct PIImport* last = &head;
                SKIP;
                while (t.kind == TK_Slash) {
                    SKIP;
                    assert(t.kind == TK_Name || t.kind == TK_Ellipsis);
                    t.spelling = Intern_insert(intern, t.spelling);
                    struct PIImport* tmp = serene_trealloc(alloc, struct PIImport);
                    assert(tmp && "OOM"), ZERO(*tmp);
                    tmp->part = t.spelling;
                    last->next = tmp;
                    last = tmp;
                    if (t.kind == TK_Ellipsis) {
                        SKIP;
                        break;
                    }
                    SKIP;
                }
                struct PIImports* tmp = serene_trealloc(alloc, struct PIImports);
                assert(tmp && "OOM"), ZERO(*tmp);
                tmp->next = data->imports;
                tmp->current = head;
                data->imports = tmp;

                assert(t.kind == TK_Semicolon);
                SKIP;
                break;
            }

            default: {
                t.spelling = Intern_insert(intern, t.spelling);
                PUSH(t);
                SKIP;
                break;
            }
            }
        }
#undef PUSH
#undef SKIP
        return data;
}

struct Ctx {
    struct serene_Trea* alloc;
    struct serene_Trea* scratch;
    struct Intern* intern;
};

static void* scan_mod_ptr(void* _ctx, void* data) {
    if (!data) return NULL;
    struct Ctx* ctx = _ctx;
    return scan_mod(ctx->alloc, ctx->scratch, ctx->intern, data);
}

static void cleanup(void* _data) {
    if (!_data) return;
    struct String* data = _data;
    // yeah we know, the const discard is needed
    munmap((char*)data->str, data->len);
}

struct MTree* scan(
    struct serene_Trea* alloc,
    struct Intern* intern,
    struct MTree* mods
) {
    struct serene_Trea scratch = serene_Trea_sub(&intern->alloc);
    struct Ctx ctx = {alloc, &scratch, intern};
    MTree_map(mods, cleanup, scan_mod_ptr, &ctx);
    serene_Trea_deinit(scratch);
    return mods;
}

static struct String cat_strings(
    struct serene_Trea* scratch,
    struct StringLL* list,
    size_t cum_len
) {
    char* string = serene_trenalloc(scratch, cum_len, char);
    assert(string && "OOM");
    size_t string_len = 0;

    for (struct StringLL* head = list; head; head = head->next) {
        size_t i = 1;
        while (true) {
            char ch = head->current.str[i];
            if (ch == '"') {
                break;
            } else if (ch == '\n') {
                string[string_len++] = '\n';
                break;
            } else if (ch == '\\') {
                i += 1;
                assert(i < head->current.len && "expected special character, got end of string literal");
                ch = head->current.str[i];
                switch (ch) {
                    case '\n': break;
                    case '\'': string[string_len++] = '\''; break;
                    case '0': string[string_len++] = '\0'; break;
                    case '"': string[string_len++] = '"'; break;
                    case 'n': string[string_len++] = '\n'; break;
                    case 't': string[string_len++] = '\t'; break;
                    default:
                        printf("unknown special character: \\%c\n", ch);
                        assert(0 && "unknown special character");
                }
            } else {
                string[string_len++] = ch;
            }
            i += 1;
        }
    }
    return (struct String){string, string_len};
}
