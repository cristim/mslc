// EXPECT: error so a return needs a value
//
// A fragment function that returns a colour has to return one.
fragment float4 bare_return_from_non_void_rejected()
{
    return;
}
