#include "./strings.h"
#include <assert.h>

static long int hash_string(const char*, size_t);

bool strings_ascii_whitespace(char c) {
    char ws[5] = { 0x09, 0x0A, 0x0C, 0x0D, 0x20 };
    for (int i = 0; i < 5; i++) {
        if (ws[i] == c) return true;
    }
    return false;
}

bool strings_ascii_digit(char c) {
    return '0' <= c && c <= '9';
}

size_t strings_strlen(const char* str) {
    size_t i = 0;
    while (str[i]) i++;
    return i;
}

bool strings_prefix_of(const char* str, const char* prefix) {
    while (*str && *prefix && *str == *prefix) {
        str++;
        prefix++;
    }
    return *prefix == '\0';
}

bool strings_equal(const char* this, const char* other) {
    while (*this && *other && *this == *other) {
        this++;
        other++;
    }
    return *this == *other;
}

static bool strings_n_equal(const char* this, const char* other, size_t len) {
    size_t i = 0;
    while (i < len && this[i] && other[i] && this[i] == other[i]) i++;
    return i == len;
}

const char* Intern_insert(struct Intern* this, struct Arena* arena, const char* s, size_t len) {
    long int hash = hash_string(s, len);
    for (struct HashesLL* head = this->hashes; head; head = head->next) {
        if (head->current.hash == hash) {
            if (strings_n_equal(head->current.string, s, len)) return head->current.string;
        }
    }

    struct HashesLL* new = arena_alloc(arena, sizeof(struct HashesLL));
    char* new_s = arena_alloc(arena, (1 + len) * sizeof(char));
    assert(new && new_s && "OOM");
    for (size_t i = 0; i < len; i++) new_s[i] = s[i];
    new_s[len] = '\0';
    new->next = this->hashes;
    new->current.string = new_s;
    new->current.hash = hash;
    this->hashes = new;
    return new_s;
}

static long int hash_string(const char* s, size_t len) {
    long int out = 0;
    for (size_t i = 0; i < len; i++) {
        // polynomial rolling hash function or smt
        // idk, it's supposed to be good
        out *= 53;
        out += s[i];
    }
    return out;
}
