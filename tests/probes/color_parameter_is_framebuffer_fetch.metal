// EXPECT: error framebuffer fetch
fragment float4 color_parameter_is_framebuffer_fetch(float4 c [[color(0)]]) { return c; }
