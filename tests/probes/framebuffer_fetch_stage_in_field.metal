// EXPECT: valid
// DISASM-MATCH: InputAttachmentIndex 0
// DISASM-MATCH: OpDecorate %[0-9A-Za-z_]+ Location 0
// DISASM-NOT: Location 1
// REFLECT: "kind": "InputAttachment", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "input_attachment_index": 0, "scalar": "float", "name": "c"

#include <metal_stdlib>
using namespace metal;

// The fetched field takes no Location: uv is the only varying, so it is Location 0 and no
// Location 1 exists. The output is at colour 2 so no output variable holds Location 0 or 1.
struct In {
  float4 c [[color(0)]];
  float2 uv;
  float4 p [[position]];
};

struct Out {
  float4 o [[color(2)]];
};

fragment Out framebuffer_fetch_stage_in_field(In in [[stage_in]]) {
  Out out;
  out.o = in.c + float4(in.uv, 0.0, 0.0);
  return out;
}
