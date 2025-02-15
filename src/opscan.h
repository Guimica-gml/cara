#ifndef OPSCAN_H
#define OPSCAN_H

#include "./lexer.h"
#include "./strings.h"
#include "./opdeclvec.h"
#include "./tokenvec.h"

void scan(
    struct Intern*,
    struct Lexer,
    struct Tokenvec* tokens_out,
    struct Opdecls* decls_out
);

#endif
