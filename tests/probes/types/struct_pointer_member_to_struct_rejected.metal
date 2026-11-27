// EXPECT: error a pointer member ("p" of "S") to a struct or to atomics is not lowered yet
// A pointer member is lowered for a scalar, vector or matrix pointee only.
struct T { float a; };
struct S { device T *p; float q; };
kernel void struct_pointer_member_to_struct_rejected(device float *o [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{ o[i] = s.q; }
