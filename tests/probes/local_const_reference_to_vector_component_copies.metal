// EXPECT: valid
// Apple binds `const thread float &r = v.y;` to a temporary copy of the component (a vector component has
// no address), so a later write to v.y is not seen through r. It was "not lowered yet"; the copy is
// pinned by a produced-value read-back (r keeps 2 while v.y becomes 50), not by this disassembly.
#include <metal_stdlib>
using namespace metal;
kernel void local_const_reference_to_vector_component_copies(device float *o [[buffer(0)]]) {
  float2 v = float2(1.0f, 2.0f);
  const thread float &r = v.y;
  v.y = 50.0f;
  o[0] = r;
}
