#include "./strings.h"
#include <assert.h>

bool strings_ascii_whitespace(char c) {
    char ws[5] = {0x09, 0x0A, 0x0C, 0x0D, 0x20};
    for (int i = 0; i < 5; i++) {
        if (ws[i] == c)
            return true;
    }
    return false;
}

bool strings_ascii_digit(char c) { return '0' <= c && c <= '9'; }

size_t cstrings_strlen(const char *str) {
    size_t i = 0;
    while (str[i])
        i++;
    return i;
}

bool strings_prefix_of(struct String str, struct String prefix) {
    if (prefix.len > str.len) return false;
    for (int i = 0; i < prefix.len; i++)
        if (str.str[i] != prefix.str[i]) return false;
    return true;
}

bool strings_equal(struct String this, struct String other) {
    if (this.len != other.len) return false;
    for (unsigned int i = 0; i < this.len; i++)
        if (this.str[i] != other.str[i]) return false;
    return true;
}

struct String strings_drop(struct String this, unsigned int num) {
    if (this.len < num) num = this.len;
    return (struct String) {
        .str = this.str + num,
        .len = this.len - num,
    };
}

struct Intern Intern_init(struct serene_Trea alloc) {
    return (struct Intern) {
        .alloc = alloc,
        .tree = {0},
    };
}

struct String Intern_insert(
    struct Intern* this,
    struct String s
) {
    struct Ordstring *entry =
        Btrings_search(&this->tree, (struct Ordstring){.str = s.str, .len = s.len});
    if (entry) return (struct String){entry->str, entry->len};

    char *new = serene_trenalloc(&this->alloc, 1 + s.len, char);
    assert(new && "OOM");
    for (size_t i = 0; i < s.len; i++)
        new[i] = s.str[i];
    new[s.len] = '\0';
    assert(Btrings_insert(
        &this->tree,
        serene_Trea_dyn(&this->alloc),
        (struct Ordstring){.str = new, .len = s.len}
    ));
    return (struct String){new, s.len};
}
