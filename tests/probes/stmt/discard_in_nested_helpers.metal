// EXPECT: valid
// A helper that discards, called through another helper.
#include <metal_stdlib>
using namespace metal;
void drop() { discard_fragment(); }
float side(float x) { drop(); return x; }
fragment float4 discard_in_nested_helpers() { return float4(side(1.0)); }
