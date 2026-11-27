// EXPECT: valid
// DISASM-MATCH: OpTypePointer PhysicalStorageBuffer %uint
// DISASM-NO-MATCH: index_t
//
// A typedef of a scalar is the scalar, as a parameter type, a local and a cast target.
#include <metal_stdlib>
using namespace metal;
typedef uint index_t;
kernel void typedef_scalar_alias(device index_t* out [[buffer(0)]], index_t i [[thread_position_in_grid]])
{ index_t v = i + 1; out[i] = v; }
