// EXPECT: valid
// DISASM-NOT: StorageBuffer8BitAccess
//
// The 8-bit storage capability is declared when a bool is in a buffer, so a
// module that only holds a bool in a local is unchanged.
kernel void bool_local_declares_no_8bit_storage(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    bool b = out[i] > 1u;
    out[i] = uint(b);
}
