// EXPECT: error an enum with a fixed underlying type is not supported
//
// Apple accepts "enum Mode : uint".
#include <metal_stdlib>
using namespace metal;
enum Mode : uint { A, B };
kernel void enum_fixed_underlying_type_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
