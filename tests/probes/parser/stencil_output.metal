// EXPECT: valid
// DISASM: OpCapability StencilExportEXT
// DISASM: OpExtension "SPV_EXT_shader_stencil_export"
// DISASM: BuiltIn FragStencilRefEXT
// DISASM: StencilRefReplacingEXT
// DISASM: BuiltIn FragDepth
// A fragment output's [[stencil]] uint is the stencil reference value (MSL 5.2.3.5, Metal 2.1), which
// Vulkan takes from FragStencilRefEXT (SPV_EXT_shader_stencil_export); it sits beside colour and depth.
#include <metal_stdlib>
using namespace metal;
struct Out { float4 c [[color(0)]]; uint s [[stencil]]; float d [[depth(any)]]; };
fragment Out f(float4 p [[position]]) { Out o; o.c = p; o.s = uint(p.x) & 0xffu; o.d = 0.5f; return o; }
