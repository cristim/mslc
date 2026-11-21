// EXPECT: valid
// DISASM: "pp_pragma_operator_once_kernel"
// _Pragma("once") is #pragma once: the second include of the header is skipped.
#include "include/pp_pragma_operator_once.h"
#include "include/pp_pragma_operator_once.h"
