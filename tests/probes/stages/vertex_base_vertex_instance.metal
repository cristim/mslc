// EXPECT: valid
// DISASM: OpCapability DrawParameters
// DISASM: BuiltIn BaseVertex
// DISASM: BuiltIn BaseInstance
// DISASM: BuiltIn VertexIndex
// DISASM: BuiltIn InstanceIndex
// [[base_vertex]] and [[base_instance]] are an indexed draw's base vertex and a draw's base
// instance, which Vulkan names vertexOffset and firstInstance. Both take a uint or a ushort.
#include <metal_stdlib>
using namespace metal;
struct Out { float4 position [[position]]; float relative; };
vertex Out vertex_base_vertex_instance(uint vid [[vertex_id]], ushort bv [[base_vertex]],
                                       uint iid [[instance_id]], uint bi [[base_instance]])
{
    Out out;
    out.position = float4(float(vid - bv), float(iid - bi), 0.0f, 1.0f);
    out.relative = float(bv);
    return out;
}
