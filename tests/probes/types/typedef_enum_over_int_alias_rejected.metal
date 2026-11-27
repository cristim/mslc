// EXPECT: error typedef redefinition with different types ('Mode' vs 'int')
//
// The reverse direction of the bug: a plain-int alias is redefined by a typedef
// of an enum, through the generic typedef path.
#include <metal_stdlib>
using namespace metal;
enum Mode { A };
typedef int Alias;
typedef Mode Alias;
kernel void typedef_enum_over_int_alias_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
