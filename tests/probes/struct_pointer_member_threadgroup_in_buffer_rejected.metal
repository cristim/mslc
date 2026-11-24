// EXPECT: error a pointer member ("p" of "S") has to point at device or constant memory to be lowered
// A buffer holds a device or constant address; a threadgroup pointer there is not lowered.
struct S { threadgroup float *p; float q; };
kernel void struct_pointer_member_threadgroup_in_buffer_rejected(device float *o [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{ o[i] = s.q; }
