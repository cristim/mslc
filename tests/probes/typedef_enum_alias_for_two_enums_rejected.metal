// EXPECT: error typedef redefinition with different types
//
// One alias for two different enums. Apple: "typedef redefinition with different types".
#include <metal_stdlib>
using namespace metal;
enum E { A };
enum F { B };
typedef enum E Alias;
typedef enum F Alias;
kernel void typedef_enum_alias_for_two_enums_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
