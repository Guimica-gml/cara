#ifndef MODULES_H
#define MODULES_H

#include "strings.h"
#include "tokenvec.h"
#include "opdeclvec.h"
#include "ast.h"
#include "serene.h"

struct Imports {
    struct ModuleNode* import;
    struct Imports* next;
};

struct Module {
    struct String contents;
    struct Tokenvec tokens;
    struct Opdecls ops;
    struct Imports *imports;
    struct Ast ast;
};

struct ModuleNode {
    struct ModuleNodesLL* children;
    char* name;
    struct Module self;
};

struct ModuleNodesLL {
    struct ModuleNodesLL *next;
    struct ModuleNode current;
};

// only gets from the top level
// we don't traverse the *entire* tree, like, christ
struct ModuleNode* ModuleNode_get(struct ModuleNode *root, struct String name);
void ModuleNode_print(struct ModuleNode, int level);
void ModuleNode_unmap(struct ModuleNode);
struct ModuleNode populate(
    struct serene_Trea*,
    char* dir_name,
    struct String dir_path
);

#endif
