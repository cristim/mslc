// EXPECT: error a pointer member ("p" of "S") is not lowered yet
// A struct with a pointer member used as a buffer was lowered with the pointer dropped
// to a scalar and q at the wrong offset (#346); it is refused until pointers are lowered.
struct S { const device float *p; float q; };
kernel void struct_pointer_member_const_device_rejected(device float *o [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{ o[i] = s.q; }
