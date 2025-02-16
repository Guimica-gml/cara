#ifndef OPDECL_H
#define OPDECL_H

#include "strings.h"

struct Opdecl {
    // values < 0 used as absence
    int lbp;
    int rbp;
    struct String token;
};

#endif
