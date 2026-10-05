// EXPECT: valid
// DISASM: OpCompositeConstruct %v3float
//
// A typedef of a vector type constructs and swizzles like the vector.
#include <metal_stdlib>
using namespace metal;
typedef float3 vec3_t;
kernel void typedef_vector_alias(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ vec3_t v = vec3_t(1.0, 2.0, 3.0); out[i] = v.y + v.z; }
