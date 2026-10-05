// EXPECT: error the enum type "Mode" is not supported as a type
//
// As a struct field.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
struct S { Mode mode; };
kernel void enum_type_as_field_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
