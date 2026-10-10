// EXPECT: valid
// A component of a bool2 reference is stored without touching its neighbour.
kernel void bool2_reference_parameter_component_store(device float *o [[buffer(0)]], device bool2 &b [[buffer(1)]]) {
  b.y = !b.y;
  o[0] = b.y ? 1.0 : 0.0;
  o[1] = b.x ? 1.0 : 0.0;
}
