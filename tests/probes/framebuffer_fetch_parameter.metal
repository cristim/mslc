// EXPECT: valid
// DISASM: OpCapability InputAttachment
// DISASM-MATCH: OpTypeImage %float SubpassData 0 0 0 2 Unknown
// DISASM-MATCH: InputAttachmentIndex 0
// DISASM-MATCH: OpImageRead %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+
// REFLECT: "kind": "InputAttachment", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "input_attachment_index": 0, "scalar": "float", "name": "c"

#include <metal_stdlib>
using namespace metal;

// Framebuffer fetch: Apple reads the colour attachment the fragment writes, here to invert it.
fragment float4 framebuffer_fetch_parameter(float4 c [[color(0)]]) {
  return float4(1.0 - c.rgb, c.a);
}
