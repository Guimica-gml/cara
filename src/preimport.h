#ifndef PREIMPORT_H
#define PREIMPORT_H

#include "tokenvec.h"
#include "opdeclvec.h"
#include "strings.h"
#include "opscan.h"
#include "mtree.h"

struct PPImport {
    struct MTree* mod;
    struct String decl;
};

struct PPImports {
    struct PPImports* next;
    struct PPImport current;
};

struct PPData {
    struct Tokenvec toks;
    struct Opdecls ops;
    struct PPImports* imports;
    enum { CS_TODO, CS_WIP, CS_DONE } closure_status;
};

void PPData_print(void*);
struct MTree* preimport(struct serene_Trea*, struct MTree*);

#endif
