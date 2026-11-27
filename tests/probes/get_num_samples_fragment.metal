// EXPECT: valid
// A fragment function reads its sample count, directly and through a helper, from one specialization constant
// the host sets from the pipeline's sample count. Its default of 1 is what a pipeline that sets none renders.
// DISASM: OpSpecConstant %uint 1
// DISASM: SpecId 262147
// REFLECT: "num_samples_spec_id": 262147
#include <metal_stdlib>
using namespace metal;
uint samples() { return metal::get_num_samples(); }
fragment float4 get_num_samples_fragment() { return float4(float(get_num_samples()), float(samples()), 0.0f, 1.0f); }
