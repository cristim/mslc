// EXPECT: error a pointer field in value struct "S" is not lowered yet
// Pins the existing value-path refusal of a pointer member in a struct nested in a local struct.
struct S { device float *p; float q; };
struct N { S s; float r; };
kernel void struct_pointer_member_nested_rejected(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ N n; n.r = 1; o[i] = n.r; }
