// EXPECT: error needs a constant integer argument
//
// The index inside the brackets has to fold as well: a name that is not a
// constant does not.
#include <metal_stdlib>
using namespace metal;
constexpr constant uint remap[] = {0, 1};
struct Out { float4 a [[color(remap[slot])]]; };
fragment Out attribute_index_constant_array_run_time_index_rejected()
{ Out o; o.a = float4(1.0); return o; }
