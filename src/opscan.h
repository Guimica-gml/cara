#ifndef OPSCAN_H
#define OPSCAN_H

#include "mtree.h"
#include "strings.h"
#include "opdeclvec.h"
#include "tokenvec.h"
#include "serene.h"

struct PIImport {
    struct PIImport* next;
    struct String part;
};

struct PIImports {
    struct PIImports* next;
    struct PIImport current;
};

struct PIData {
    struct Tokenvec toks;
    struct Opdecls ops;
    struct PIImports* imports;
};

void PIData_print(void*);
struct MTree* scan(struct serene_Trea* alloc, struct Intern*, struct MTree*);

#endif
