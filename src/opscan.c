#include "./opscan.h"
#include <assert.h>
#include <stdio.h>

struct StringLL {
    struct StringLL* next;
    const char* current;
    size_t len;
};

static char* cat_strings(
    struct serene_Allocator scratch,
    struct StringLL* list,
    size_t cum_len,
    size_t *out_len
);

void scan(
    struct serene_Allocator scratch,
    struct Intern* intern,
    struct Lexer lexer,
    struct Tokenvec* tokens_out,
    struct Opdecls* decls_out
) {
#define SKIP r = Lexer_next(&lexer)
    struct LexResult r = Lexer_next(&lexer);
    while (r.token.kind != TK_EOF) {
        if (r.token.kind == TK_String) {
            struct StringLL* list = serene_alloc(scratch, struct StringLL);
            assert(list && "OOM");
            list->next = NULL;
            list->current = r.token.spelling;
            list->len = r.len;
            struct StringLL* last = list;
            size_t cum_len = r.len;

            for (SKIP; r.token.kind == TK_String; SKIP) {
                struct StringLL* tmp = serene_alloc(scratch, struct StringLL);
                assert(tmp && "OOM");
                *tmp = (typeof(*tmp)){0};
                tmp->current = r.token.spelling;
                tmp->len = r.len;
                last->next = tmp;
                last = tmp;
                cum_len += r.len;
            }

            char* whole = cat_strings(scratch, list, cum_len, &cum_len);
            const char* spelling = Intern_insert(intern, whole, cum_len);
            assert(Tokenvec_push(
                tokens_out,
                (struct Token){.kind = TK_String, .spelling = spelling, .number = 0}
            ));
            continue;
        }

        if (r.token.kind == TK_Operator) {
            SKIP;
            assert(r.token.kind == TK_OpenParen), SKIP;
            int lbp;
            if (r.token.kind == TK_Number) {
                lbp = r.token.number;
            } else {
                lbp = -1;
                assert(r.token.spelling[0] == '_');
                assert(r.len == 1);
            }
            SKIP;
            assert(r.token.kind == TK_Comma), SKIP;
            const char* name = Intern_insert(intern, r.token.spelling, r.len);
            assert(r.token.kind == TK_Name), SKIP;
            assert(r.token.kind == TK_Comma), SKIP;
            int rbp;
            if (r.token.kind == TK_Number) {
                rbp = r.token.number;
            } else {
                rbp = -1;
                assert(r.token.spelling[0] == '_');
                assert(r.len == 1);
            }
            SKIP;
            assert(r.token.kind == TK_CloseParen), SKIP;

            Opdecls_push(decls_out, (struct Opdecl){.token = name, .lbp = lbp, .rbp = rbp});
            continue;
        }

        r.token.spelling = Intern_insert(intern, r.token.spelling, r.len);
        assert(Tokenvec_push(tokens_out, r.token));
        SKIP;
    }
#undef SKIP
}

static char* cat_strings(
    struct serene_Allocator scratch,
    struct StringLL* list,
    size_t cum_len,
    size_t *out_len
) {
    char *string = serene_nalloc(scratch, cum_len, char);
    assert(string && "OOM");
    size_t string_len = 0;

    for (struct StringLL* head = list; head; head = head->next) {
        size_t i = 1;
        while (true) {
            char ch = head->current[i];
            if (ch == '"') {
                break;
            } else if (ch == '\n') {
                string[string_len++] = '\n';
                break;
            } else if (ch == '\\') {
                i += 1;
                assert(i < head->len && "expected special character, got end of string literal");
                ch = head->current[i];
                assert(ch != '\n' && "expected special character, got end of string literal");
                switch (ch) {
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
    *out_len = string_len;
    return string;
}
