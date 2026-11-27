// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: a template in a namespace, reached through a using declaration.
#include <metal_stdlib>
using namespace metal;
namespace util { template <typename T> T twice(T x) { return x + x; } }
using util::twice;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = twice(1.0f); }
