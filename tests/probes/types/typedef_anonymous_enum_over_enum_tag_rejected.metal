// EXPECT: error redefinition of "Mode"
//
// A second, anonymous enum under the name of an existing one is a different type. Apple: "typedef redefinition with different types".
#include <metal_stdlib>
using namespace metal;
enum Mode { A };
typedef enum { B } Mode;
kernel void typedef_anonymous_enum_over_enum_tag_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
