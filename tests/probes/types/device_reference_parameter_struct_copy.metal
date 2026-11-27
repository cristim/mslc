// EXPECT: valid
// A whole struct is copied from one buffer reference to another.
#include <metal_stdlib>
using namespace metal;
struct S { float4 c; float k; };
kernel void device_reference_parameter_struct_copy(device S &o [[buffer(0)]], constant S &x [[buffer(1)]]) {
  o = x;
}
