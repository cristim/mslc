// EXPECT: error sizeof of this expression is not lowered
// The pointer itself is part of the struct, which is in constant memory, so assigning it is not an
// expression sizeof can take, though the pointee is writable.
struct S { device float *p; };
kernel void struct_pointer_member_sizeof_assign_rejected(constant S &s [[buffer(0)]], device float *o [[buffer(1)]])
{ o[0] = sizeof(s.p = s.p); }
