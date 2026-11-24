// EXPECT: error the pointer member "p" is only lowered when indexed or dereferenced
// Pointer arithmetic and comparisons on a pointer member are not lowered.
struct S { device float *p; };
kernel void struct_pointer_member_as_value_rejected(constant S &s [[buffer(0)]], device float *o [[buffer(1)]])
{ o[0] = (s.p + 1)[0]; }
