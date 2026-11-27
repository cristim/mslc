// EXPECT: valid
// DISASM-MATCH: OpTypeImage %float SubpassData 0 0 0 2 Unknown
// DISASM-MATCH: OpTypeImage %int SubpassData 0 0 0 2 Unknown
// DISASM-MATCH: OpTypeImage %uint SubpassData 0 0 0 2 Unknown
// DISASM-MATCH: InputAttachmentIndex 2
// REFLECT: "metal_index": 2, "descriptor": { "set": 1, "binding": 0 }, "input_attachment_index": 2, "scalar": "float", "name": "h"
// REFLECT: "metal_index": 3, "descriptor": { "set": 1, "binding": 1 }, "input_attachment_index": 3, "scalar": "int", "name": "i"
// REFLECT: "metal_index": 7, "descriptor": { "set": 1, "binding": 2 }, "input_attachment_index": 7, "scalar": "uint", "name": "u"

#include <metal_stdlib>
using namespace metal;

// half crosses as a float; int and uint keep their kind. The parameters are declared out of order; the bindings run in colour order.
struct Out {
  half4 h [[color(2)]];
  int4 i [[color(3)]];
  uint4 u [[color(7)]];
};

fragment Out framebuffer_fetch_scalar_kinds(uint4 u [[color(7)]], half4 h [[color(2)]], int4 i [[color(3)]]) {
  Out out;
  out.h = h + half4(1.0h);
  out.i = i + 1;
  out.u = u + 1;
  return out;
}
