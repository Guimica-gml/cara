#ifndef OPSCAN_H
#define OPSCAN_H

#include "./lexer.h"
#include "./strings.h"
#include "./opdeclvec.h"
#include "./tokenvec.h"
#include "modules.h"

void scan(
    struct Intern*,
    struct ModuleNode* mod,
    struct ModuleNode* parent
);

#endif
