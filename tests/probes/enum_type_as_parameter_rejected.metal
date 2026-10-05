// EXPECT: error the enum type "Mode" is not supported as a type
//
// As a buffer element type, which is what a header shared with C would do.
#include <metal_stdlib>
using namespace metal;
typedef enum Mode { A, B } Mode;
kernel void enum_type_as_parameter_rejected(device Mode* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = B; }
