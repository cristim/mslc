// EXPECT: valid
// DISASM-MATCH: OpKill
// discard_fragment() in a helper drops the fragment of the fragment function that calls it
// (lavapipe readback of a 16x16 target: the discarded half keeps its clear value, the rest is
// written). Apple accepts the helper.
#include <metal_stdlib>
using namespace metal;
float maybe_drop(float x) { if (x > 0.5f) discard_fragment(); return x; }
fragment float4 discard_in_a_helper(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return float4(maybe_drop(0.75f));
}
