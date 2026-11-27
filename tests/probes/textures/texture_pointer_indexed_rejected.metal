// EXPECT: error is dereferenced (*p or p[0]) only, not indexed
// Apple accepts p[1] as pointer arithmetic, undefined here; mslc refuses it.
#include <metal_stdlib>
using namespace metal;

fragment float4 f(texture2d<float> t [[texture(0)]], texture2d<float> u [[texture(1)]], sampler s [[sampler(0)]], float4 c [[position]]) { thread texture2d<float>* p = &t; return p[1].sample(s, float2(0.5)); }
