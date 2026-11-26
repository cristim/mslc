// EXPECT: error assigning to ".x" of "p", which is const or in constant memory
// p points at const device memory, which Metal does not let a kernel write.
struct S { const device float2 *p; };
kernel void struct_pointer_member_const_device_store_rejected(constant S &s [[buffer(0)]], uint i [[thread_position_in_grid]])
{ s.p[i].x = 1; }
