// EXPECT: valid
// uchar, char, short and ushort, scalar and vector, cross in 32 bits and are narrowed on the way in
// (lavapipe draw: 200, -3, -1000, 60000 and uchar2(7, 9) arrive exact; Apple accepts them).
struct VOut { float4 position [[position]]; uchar a; char b; short c; ushort d; uchar2 e; };
vertex VOut stage_narrow_integer_fields_cross_vs(uint vid [[vertex_id]]) { VOut o; o.position = float4(0.0, 0.0, 0.0, 1.0); o.a = uchar(200); o.b = char(-3); o.c = short(-1000); o.d = ushort(60000); o.e = uchar2(7, 9); return o; }
fragment float4 stage_narrow_integer_fields_cross_fs(VOut in [[stage_in]]) { return float4(float(in.a), float(in.b), float(in.c), float(in.d) + float(in.e.x + in.e.y)); }
