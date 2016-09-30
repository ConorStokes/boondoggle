void main( uint vertexIndex : SV_VERTEXID, out float4 position : SV_POSITION, out float2 texCoord : TEXCOORD0 )
{
    texCoord.x = vertexIndex == 2 ? 2.0f : 0.0f;
    texCoord.y = vertexIndex == 0 ? -1.0f : 1.0f;

    position.x = vertexIndex == 2 ? 3.0f : -1.0f;
    position.y = vertexIndex == 0 ? 3.0f : -1.0f;
    position.zw = float2( 0.0f, 1.0f );
}