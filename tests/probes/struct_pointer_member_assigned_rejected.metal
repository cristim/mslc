// EXPECT: error the pointer member "p" is only lowered when indexed or dereferenced
// Assigning the pointer itself is not lowered; it used to reach a conversion to a pointer type.
struct S { device float *p; };
kernel void struct_pointer_member_assigned_rejected(device S &s [[buffer(0)]])
{ s.p = 0; }
