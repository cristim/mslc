// EXPECT: error a local in the threadgroup address space is not lowered yet
//
// Direct initialisation reaches an address space. "threadgroup float3 v(0);" is
// legal MSL that xcrun metal accepts, and mslc has to refuse it: every local
// mslc declares goes in StorageClass Function, so the module this used to emit
// held
//
//   %47 = OpVariable %_ptr_Function_v3float Function
//
// which validates and is a thread-private local in a source that says the value
// is shared across the threadgroup. Nothing downstream reports it.
//
// The "device" spelling is the other way round and the same guard stops it:
// xcrun metal rejects "device float3 v(0);" as an automatic variable qualified
// with an address space, and mslc used to accept it.
//
// "threadgroup float3 v;" and "threadgroup float3 v = float3(0);" were already
// refused before this change, by a different diagnostic, so the direct
// initialisation spelling was the only remaining route to the wrong module.
kernel void construct_an_address_space_local_not_lowered(
    device float3 *out [[buffer(0)]],
    uint index [[thread_position_in_grid]])
{
    threadgroup float3 v(0);
    out[index] = v;
}
