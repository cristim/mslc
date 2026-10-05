// EXPECT: error a struct of several colour attachments is not lowered yet
//
// A fragment function returning a struct writes one colour attachment per
// [[color(n)]] field, which is its own capability.
struct Colors { float4 a; };

fragment Colors fragment_returning_a_struct_rejected()
{
    Colors c;
    c.a = float4(1.0);
    return c;
}
