// EXPECT: valid
// A helper's body is emitted once for the fragment functions that share it, and each entry point that reaches
// it reads the sample count, so the reflection of both names the constant, the second's included.
// REFLECT-COUNT: 2 "num_samples_spec_id": 262147
#include <metal_stdlib>
using namespace metal;
uint samples() { return get_num_samples(); }
fragment float4 first() { return float4(float(samples())); }
fragment float4 second() { return float4(float(samples())); }
