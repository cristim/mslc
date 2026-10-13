// EXPECT: valid
// DISASM: OpReturnValue %int_7
// A::abs's parameter is an unscoped enum, not a plain int: an enum is an exact match
// for its own parameter (Apple and master both call A's), not one of the library's
// scalar/vector signatures. Enums lower to OpTypeInt in SPIR-V, so without an
// explicit enumName check both the parameter list and the call's argument read as
// plain int and wrongly match the library's abs.
#include <metal_stdlib>
using namespace metal;
enum E { ea, eb };
namespace A { int abs(E x) { return 7; } }
using namespace A;
kernel void stdlib_name_enum_parameter_not_a_library_match_accepted(device int* out [[buffer(0)]])
{ out[0] = abs(ea); }
