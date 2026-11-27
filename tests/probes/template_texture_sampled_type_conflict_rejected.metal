// EXPECT: error no matching function for call to "both"
// T is deduced as float from one texture and as int from the other, so the template is no candidate.
#include <metal_stdlib>
using namespace metal;
template <typename T> float both(texture2d<T> a, texture2d<T> b) { return 1.0f; }
fragment float4 f(texture2d<float> tf [[texture(0)]], texture2d<int> ti [[texture(1)]]) { return float4(both(tf, ti)); }
