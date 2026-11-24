// EXPECT: error cannot store through "p", which is in the constant address space or declared const
// p points at constant memory, which Metal does not let a kernel write.
struct S { constant float *p; };
kernel void struct_pointer_member_constant_store_rejected(device S &s [[buffer(0)]], uint i [[thread_position_in_grid]])
{ s.p[i] = 1; }
