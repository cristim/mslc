// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpLoad %_ptr_PhysicalStorageBuffer__struct_[0-9]+ %[0-9]+ Aligned 8
// DISASM-MATCH-CAPTURED: (%[0-9]+) = OpAccessChain %_ptr_PhysicalStorageBuffer_float(_[0-9]+)? $1 %uint_0(_[0-9]+)? %[0-9]+
// The struct holding p is in constant memory, but what p points at is device memory: the pointer is
// loaded from the struct (8-byte aligned) and the store goes through it, as Metal 3 code passes a
// buffer's gpuAddress in an argument.
struct vector_arg { device float *ptr; long len; };
kernel void struct_pointer_member_store_through_constant_struct(constant vector_arg &v [[buffer(0)]],
    constant float &value [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (i < uint(v.len))
        v.ptr[i] = value;
}
