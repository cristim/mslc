// EXPECT: error from a body that is one return statement
// Apple accepts it; mslc resolves such a call from a single return statement.
#include <metal_stdlib>
using namespace metal;
thread texture2d<float>* addressOf(thread texture2d<float>& r) { thread texture2d<float>* p = &r; return p; }
fragment float4 f(texture2d<float> t [[texture(0)]], texture2d<float> u [[texture(1)]], sampler s [[sampler(0)]], float4 c [[position]]) { return addressOf(t)->sample(s, float2(0.5)); }
