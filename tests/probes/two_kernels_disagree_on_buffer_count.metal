// EXPECT: error two entry points in the same stage bind 2 and 3 buffers
// Two kernels in one source binding a different number of buffers. indium packs
// an entry point's buffer addresses into one address block at binding 0, and a
// module has one such variable per set, so two kernels in the same stage share
// it and the member list is fixed when the first one is built.
//
// The second kernel's third buffer would then index a member that is not there.
// That is worth a hard error rather than a silent reuse: spirv-val does catch
// the out-of-range access chain, so the failure arrives as a rejected module
// rather than a wrong answer, but only because the indices happen to be checked.
// The failure mode this guards is the one where they are not.
kernel void k_two_buffers(device float* a [[buffer(0)]],
                          device float* b [[buffer(1)]],
                          uint i [[thread_position_in_grid]])
{
    a[i] = b[i] * 2.0;
}

kernel void k_three_buffers(device float* c [[buffer(0)]],
                            device float* d [[buffer(1)]],
                            device float* e [[buffer(2)]],
                            uint i [[thread_position_in_grid]])
{
    c[i] = d[i] + e[i];
}
