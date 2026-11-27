// EXPECT: error a constant expression that mixes an unsigned operand with a negative value
//
// -1 + 0u is unsigned arithmetic on a negative value, which wraps and which the folder does not model.
#include <metal_stdlib>
using namespace metal;
enum One { A = -1 + 0u };
kernel void enum_value_unsigned_operand_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
