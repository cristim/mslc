// EXPECT: error after an enum declaration
//
// enum Mode { A } m; declares a variable of the enum type.
#include <metal_stdlib>
using namespace metal;
enum Mode { A } m;
kernel void enum_variable_declarator_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
