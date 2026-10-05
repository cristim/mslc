// EXPECT: error an unsigned or wider integer inside a constant expression is not supported
//
// Mixing u operands changes the arithmetic to unsigned, which the folder does not model.
#include <metal_stdlib>
using namespace metal;
enum One { A = -1 + 0u };
kernel void enum_value_unsigned_operand_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
