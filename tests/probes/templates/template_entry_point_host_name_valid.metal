// EXPECT: valid
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "k_float"
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "k_int"
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "k_deduced"
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "k_nontype"
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "k_partial"
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "k_ns"
// REFLECT: "name": "k_float"
// REFLECT: "name": "k_int"
// REFLECT: "name": "k_deduced"
// REFLECT: "name": "k_nontype"
// REFLECT: "name": "k_partial"
// REFLECT: "name": "k_ns"
// RUN: grid=64 local=8
#include <metal_stdlib>
using namespace metal;
float k_def(float x) { return x * 2.0f; }
template <typename T> kernel void k(device T* out [[buffer(0)]], device const T* in [[buffer(1)]],
	uint i [[thread_position_in_grid]]) { out[i] = in[i] + T(2); }
template <typename T, int N = 3> kernel void k_def(device T* out [[buffer(0)]],
	uint i [[thread_position_in_grid]]) { out[i] = T(k_def(float(i))) + T(N); }
template <typename BiasT, typename T> kernel void k_partial(device T* out [[buffer(0)]],
	uint i [[thread_position_in_grid]]) { out[i] = T(BiasT(i) + BiasT(4)); }
namespace N {
template <typename T> kernel void k_in(device T* out [[buffer(0)]],
	uint i [[thread_position_in_grid]]) { out[i] = T(i) * T(2) + T(1); }
}
template [[host_name("k_float")]] kernel void k<float>(device float*, device const float*, uint);
template [[host_name("k_int")]] kernel void k<int>(device int* out [[buffer(0)]], device const int*, uint);
template [[host_name("k_deduced")]] kernel void k_def(device float*, uint);
template [[host_name("k_nontype")]] kernel void k_def<float, 5>(device float*, uint);
template [[host_name("k_partial")]] kernel void k_partial<int>(device float*, uint);
template [[host_name("k_ns")]] kernel void ::N::k_in<float>(device float*, uint);
