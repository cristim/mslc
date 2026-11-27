// EXPECT: error redefinition of "A"
//
// Apple: "redefinition of enumerator 'A'".
#include <metal_stdlib>
using namespace metal;
enum One { A };
enum Two { A };
kernel void enum_constant_redefined_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
