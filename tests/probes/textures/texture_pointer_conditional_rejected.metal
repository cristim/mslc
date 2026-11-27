// EXPECT: error a conditional, a member or an element would pick the descriptor at run time
// Apple accepts this; a descriptor chosen at run time is not lowered.
#include <metal_stdlib>
using namespace metal;

fragment float4 f(texture2d<float> t [[texture(0)]], texture2d<float> u [[texture(1)]], sampler s [[sampler(0)]], float4 c [[position]]) { thread texture2d<float>* p = c.x > 0 ? &t : &u; return p->sample(s, float2(0.5)); }
