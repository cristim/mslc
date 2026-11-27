// EXPECT: error which cannot be assigned
// Apple accepts this; mslc binds a pointer to one descriptor and does not lower choosing another at run time.
#include <metal_stdlib>
using namespace metal;

fragment float4 f(texture2d<float> t [[texture(0)]], texture2d<float> u [[texture(1)]], sampler s [[sampler(0)]], float4 c [[position]]) { thread texture2d<float>* p = &t; p = &u; return p->sample(s, float2(0.5)); }
