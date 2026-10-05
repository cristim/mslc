// EXPECT: error a local of array type float[3] is not lowered yet
//
// An enumerator is a constant array length. Apple rejects "float[Count] v;" ("brackets are not allowed here"), so this pins only that the length folded to 3 before the existing refusal.
#include <metal_stdlib>
using namespace metal;
enum { Count = 3 };
kernel void enum_in_array_length_reaches_the_lowering_check(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float[Count] v; out[i] = i; }
