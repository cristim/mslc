// EXPECT: error cannot store through
// A constant T& parameter is read only.
#include <metal_stdlib>
using namespace metal;
kernel void constant_reference_parameter_store_rejected(constant float &x [[buffer(0)]]) {
  x = 1.0;
}
