// EXPECT: valid
// RUN: grid=1 local=1
// DIFFCHECK-RANGE: p 0 63
struct Params { float scale; uint offset; };
kernel void constant_struct_ref(constant Params& p [[buffer(0)]],
                                device float* out [[buffer(1)]],
                                uint i [[thread_position_in_grid]])
{ out[i + p.offset] = out[i] * p.scale; }
