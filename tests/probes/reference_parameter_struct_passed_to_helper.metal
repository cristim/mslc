// EXPECT: valid
// A constant struct& parameter passed by value to a helper function.
#include <metal_stdlib>
using namespace metal;
struct S { float4 c; float k; };
float4 scale(S s) { return s.c * s.k; }
fragment float4 reference_parameter_struct_passed_to_helper(constant S &p [[buffer(0)]]) {
  return scale(p);
}
