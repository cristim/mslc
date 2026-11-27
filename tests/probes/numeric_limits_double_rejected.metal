// EXPECT: error numeric_limits<double> is not lowered yet
// mslc has no double literal, so a double's limits would be built from a float and come out as 0 or infinity.
#include <metal_stdlib>
using namespace metal;
kernel void numeric_limits_double_rejected(device float* out [[buffer(0)]]) { out[0] = float(numeric_limits<double>::epsilon()); }
