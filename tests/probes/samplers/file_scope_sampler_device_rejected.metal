// EXPECT: error only a constant can be declared at file scope
// Apple: program scope variable must reside in constant address space.
#include <metal_stdlib>
using namespace metal;
device sampler S(filter::linear);
fragment float4 f() { return float4(0.0); }
