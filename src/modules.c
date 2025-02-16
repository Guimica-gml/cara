#include "modules.h"

#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

struct ModuleNode* ModuleNode_get(struct ModuleNode* root, struct String name) {
    for (struct ModuleNodesLL* head = root->children; head; head = head->next) {
        if (strncmp(head->current.name, name.str, name.len) == 0) {
            return &head->current;
        }
    }
    return NULL;
}

void ModuleNode_print(struct ModuleNode this, int level) {
    for (int i = 0; i < level; i++) printf("  ");
    printf("%s (%p)\n", this.name, this.self.contents.str);
    for (struct ModuleNodesLL* head = this.children; head; head = head->next) {
        ModuleNode_print(head->current, level + 1);
    }
}

void ModuleNode_unmap(struct ModuleNode tree) {
    if (tree.self.contents.str) munmap(
        (void*) tree.self.contents.str,
        tree.self.contents.len
    );
    for (struct ModuleNodesLL* head = tree.children; head; head = head->next)
        ModuleNode_unmap(head->current);
}

static struct ModuleNode populate_internal(
    struct serene_Trea* alloc,
    struct serene_Trea* scratch,
    char* dir_name,
    const char* dir_path,
    int dir_pathlen
) {
    struct ModuleNode out = {0};
    out.name = dir_name;
    DIR *dir = opendir(dir_path);
    for (
        struct dirent* entry = readdir(dir);
        entry;
        entry = readdir(dir)
    ) {
        if (entry->d_name[0] == '.') continue;
        int namelen = strlen(entry->d_name);
        char* full_path = serene_trenalloc(scratch, namelen + dir_pathlen + 2, char);
        assert(full_path && "OOM");
        snprintf(full_path, namelen + dir_pathlen + 2, "%s/%s", dir_path, entry->d_name);

        int fd = open(full_path, O_RDWR);
        if (fd != -1) {
            if (strncmp(entry->d_name + namelen - 5, ".tara", 5) != 0) {
                close(fd);
                continue;
            }
            struct stat stat = {0};
            assert(fstat(fd, &stat) == 0);
            struct ModuleNode child = {0};
            child.name = serene_trenalloc(alloc, namelen - 4, char);
            snprintf(child.name, namelen - 4, "%s", entry->d_name);
            child.self.contents.len = stat.st_size;
            child.self.contents.str = mmap(
                NULL,
                stat.st_size,
                PROT_READ,
                MAP_PRIVATE,
                fd,
                0
            );

            struct ModuleNodesLL* tmp = serene_trealloc(alloc, struct ModuleNodesLL);
            assert(tmp && "OOM");
            tmp->current = child;
            tmp->next = out.children;
            out.children = tmp;
        } else {
            struct ModuleNode child = populate_internal(
                alloc,
                scratch,
                entry->d_name,
                full_path,
                namelen + dir_pathlen + 1
            );
            struct ModuleNodesLL* head;
            for (head = out.children; head; head = head->next) {
                if (strcmp(head->current.name, child.name) == 0) {
                    assert(head->current.children == NULL);
                    head->current.children = child.children;
                    break;
                }
            }
            if (head == NULL) {
                struct ModuleNodesLL* tmp = serene_trealloc(alloc, struct ModuleNodesLL);
                assert(tmp && "OOM");
                tmp->current = child;
                tmp->next = out.children;
                out.children = tmp;
            }
        }
    }
    return out;
}

struct ModuleNode populate(
    struct serene_Trea* alloc,
    char* dir_name,
    struct String dir_path
) {
    struct serene_Trea scratch = serene_Trea_sub(alloc);
    struct ModuleNode out = populate_internal(alloc, &scratch, dir_name, dir_path.str, dir_path.len);
    serene_Trea_deinit(scratch);
    return out;
}
