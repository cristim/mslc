// EXPECT: error an atomic member of a struct
// Apple rejects a plain read of the member; mslc does not lower atomic members, so it says so.
struct S { atomic_uint n; uint x; };
kernel void atomic_struct_member_rejected(device uint *o [[buffer(0)]], device S *c [[buffer(1)]]) {
  o[0] = c->x;
}
