// EXPECT: error framebuffer fetch
// Apple accepts a [[color(n)]] stage_in field. Vulkan has no equivalent except
// a subpass input attachment, which needs a descriptor mslc does not assign.
struct I { float4 c [[color(0)]]; };
fragment float4 color_on_fragment_input_is_framebuffer_fetch(I in [[stage_in]]) { return in.c; }
