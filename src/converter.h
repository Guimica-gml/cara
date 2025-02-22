#ifndef CONVERTER_H
#define CONVERTER_H

#include "./ast.h"
#include "./symbols.h"
#include "./tst.h"
#include "parser.h"
#include "serene.h"

struct Tst convert_ast(struct serene_Trea*, struct PTData* main);

#endif
