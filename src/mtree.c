#include "mtree.h"
#include "commons.h"
#include "common_ll.h"

#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdbool.h>

static void print_leveled(struct MTree* this, void (*p)(void*), int level) {
    for (int i = 0; i < level; i++) printf("| ");
    printf("%.*s data: (%p) ", (int)this->name.len, this->name.str, this->data);
    if (this->data) p(this->data);
    printf("\n");
    for (int i = 0; i < this->subs_count; i++)
        print_leveled(&this->subs[i], p, level+1);
}

void MTree_print(struct MTree* this, void (*p)(void*)) { print_leveled(this, p, 0); }

void MTree_map(
    struct MTree* this,
    void (*c)(void*),
    void* (*f)(void*, void*),
    void* ctx
) {
    for (size_t i = 0; i < this->subs_count; i++)
        MTree_map(&this->subs[i], c, f, ctx);
    void* new_data = f(ctx, this->data);
    c(this->data);
    this->data = new_data;
}

void MTree_map_whole(
    struct MTree* this,
    void (*f)(void*, struct MTree*),
    void* ctx
) {
    for (size_t i = 0; i < this->subs_count; i++)
        MTree_map_whole(&this->subs[i], f, ctx);
    f(ctx, this);
}

struct MTree* MTree_index(struct MTree* this, struct String idx) {
    for (size_t i = 0; i < this->subs_count; i++) {
        if (strings_equal(idx, this->subs[i].name))
            return &this->subs[i];
    }
    return NULL;
}


static void from_file(
    struct MTree* out,
    struct serene_Trea* alloc,
    int fd,
    struct stat* stat
) {
    struct String* data = serene_trealloc(alloc, struct String);
    assert(data && "OOM"), ZERO(*data);
    data->str = mmap(NULL, stat->st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    data->len = stat->st_size;
    out->data = data;
}

static int count_entries(DIR* dir) {
    int count = 0;
    for (
        struct dirent* entry = readdir(dir);
        entry;
        entry = readdir(dir)
    ) count++;
    rewinddir(dir);
    return count;
}

static struct Row {
    struct MTree* subs;
    size_t subs_count;
} row_from_dir(
    struct serene_Trea* alloc,
    int dir_fd
) {
    DIR* dir = fdopendir(dir_fd);
    int alloc_size = count_entries(dir);
    struct MTree* subs = serene_trenalloc(alloc, alloc_size, struct MTree);
    assert(subs && "OOM");
    size_t subs_count = 0;
    for (struct dirent* entry = readdir(dir); entry; entry = readdir(dir)) {
        if (entry->d_name[0] == '.') continue;
        struct MTree* slot = &subs[subs_count];
        ZERO(*slot);
        int entry_fd = openat(dir_fd, entry->d_name, O_RDONLY);
        struct stat stat;
        assert(fstat(entry_fd, &stat) == 0);
        bool is_dir = S_ISDIR(stat.st_mode);
        int len = strlen(entry->d_name);
        bool ext = is_dir || 0 == strcmp(entry->d_name + len - 5, ".tara");
        if (!ext) {
            close(entry_fd);
            continue;
        }
        if (!is_dir) { len -= 5; }
        struct String search = {entry->d_name, len};
        for (int j = 0; j < subs_count; j++) {
            if (!strings_equal(subs[j].name, search)) continue;
            slot = &subs[j];
            subs_count--;
            goto after;
        }
        char* str = serene_trenalloc(alloc, len + 1, char);
        assert(str && "OOM");
        snprintf(str, len + 1, "%s", entry->d_name);
        slot->name = (struct String){str, len};
    after:
        if (is_dir) {
            struct Row row = row_from_dir(alloc, entry_fd);
            slot->subs = row.subs;
            slot->subs_count = row.subs_count;
        } else {
            from_file(slot, alloc, entry_fd, &stat);
        }
        close(entry_fd);
        subs_count++;
    }
    return (struct Row){subs, subs_count};
}

struct MTree* MTree_load(
    struct serene_Trea* alloc,
    struct String dir_path
) {
    struct MTree* out = serene_trealloc(alloc, struct MTree);
    assert(out && "OOM"), ZERO(*out);
    out->name = dir_path;
    int dir_fd = open(dir_path.str, O_RDONLY | O_DIRECTORY);
    assert(dir_fd != -1);
    struct Row row = row_from_dir(alloc, dir_fd);
    out->subs = row.subs;
    out->subs_count = row.subs_count;
    close(dir_fd);
    return out;
}

