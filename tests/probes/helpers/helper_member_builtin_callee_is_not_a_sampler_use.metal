// EXPECT: valid
// REFLECT-NOT: "embedded_sampler": 0
#include <metal_stdlib>
using namespace metal;
struct Lookup { float value; float look(float x) const { return sin(x); } };
constexpr sampler sin(filter::linear);
fragment float4 f() { Lookup l; l.value = 1.0; return float4(l.look(0.5)); }
