// EXPECT: error a scoped enum (enum class) is not supported
//
// Apple accepts enum class; its enumerators are reached as Name::A, which mslc does not parse.
#include <metal_stdlib>
using namespace metal;
enum class Mode { A, B };
kernel void enum_class_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
