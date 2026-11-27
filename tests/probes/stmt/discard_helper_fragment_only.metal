// EXPECT: valid
// A helper with discard that only a fragment function calls does not stop a vertex
// function in the same file that calls a different helper.
#include <metal_stdlib>
using namespace metal;
float maybe_drop(float x) { if (x > 0.5f) discard_fragment(); return x; }
float plain(float x) { return x; }
fragment float4 discard_helper_fragment_only_a() { return float4(maybe_drop(0.1)); }
vertex float4 discard_helper_fragment_only_v(uint i [[vertex_id]]) { return float4(plain(1.0)); }
