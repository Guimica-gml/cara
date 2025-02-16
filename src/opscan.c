#include "./opscan.h"
#include <assert.h>
#include <stdio.h>

struct StringLL {
    struct StringLL* next;
    struct String current;
};

static struct String cat_strings(
    struct serene_Trea* scratch,
    struct StringLL* list,
    size_t cum_len
);

void scan(
    struct Intern* intern,
    struct Lexer lexer,
    struct Tokenvec* tokens_out,
    struct Opdecls* decls_out
) {
    struct serene_Trea scratch = serene_Trea_sub(&intern->alloc);
#define SKIP t = Lexer_next(&lexer)
    struct Token t = Lexer_next(&lexer);
    while (t.kind != TK_EOF) {
        if (t.kind == TK_String) {
            struct StringLL* list = serene_trealloc(&scratch, struct StringLL);
            assert(list && "OOM");
            list->next = NULL;
            list->current = t.spelling;
            struct StringLL* last = list;
            size_t cum_len = t.spelling.len;

            for (SKIP; t.kind == TK_String; SKIP) {
                struct StringLL* tmp = serene_trealloc(&scratch, struct StringLL);
                assert(tmp && "OOM");
                *tmp = (typeof(*tmp)){0};
                tmp->current = t.spelling;
                last->next = tmp;
                last = tmp;
                cum_len += t.spelling.len;
            }

            struct String whole = cat_strings(&scratch, list, cum_len);
            struct String spelling = Intern_insert(intern, whole);
            assert(Tokenvec_push(
                tokens_out,
                (struct Token){.kind = TK_String, .spelling = spelling, .number = 0}
            ));
            continue;
        }

        if (t.kind == TK_Comment) {
            SKIP;
            continue;
        }

        if (t.kind == TK_Operator) {
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

            Opdecls_push(decls_out, (struct Opdecl){.token = name, .lbp = lbp, .rbp = rbp});
            continue;
        }

        t.spelling = Intern_insert(intern, t.spelling);
        assert(Tokenvec_push(tokens_out, t));
        SKIP;
    }
#undef SKIP
}

static struct String cat_strings(
    struct serene_Trea* scratch,
    struct StringLL* list,
    size_t cum_len
) {
    char *string = serene_trenalloc(scratch, cum_len, char);
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
