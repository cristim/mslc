// EXPECT: valid
// DISASM: "pp_pragma_operator_once_arg_kernel"
// _Pragma("once") passed through a macro argument still marks the header once.
#include "include/pp_pragma_operator_once_in_an_argument.h"
#include "include/pp_pragma_operator_once_in_an_argument.h"
