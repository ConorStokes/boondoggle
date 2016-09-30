float SphereSDF( float3 centre, float r, float3 from )
{
    return length( from - centre ) - r;
}

float BoxSDF( float3 minPosition, float3 bounds, float3 from )
{
    float3 d = abs( from - minPosition ) - bounds;

    return min( max( d.x, max( d.y, d.z ) ), 0.0f ) + length( max( d, 0.0f ) );
}

float RoundBoxSDF( float3 minPosition, float3 bounds, float distance, float3 from )
{
    return BoxSDF( minPosition, bounds, from ) - distance;
}

float PlainSDF( float3 normal, float d, float3 from )
{
    return dot( normal, from ) - d;
}

void UnionSDF( inout float2 currentSDF, float2 newSDF )
{
    currentSDF = currentSDF.x < newSDF.x ? currentSDF : newSDF;
}

// Creates a capsule on the x axis
float CapsuleSDF( float lineLength, float r, float3 from )
{
    return length( float3( from.x - clamp( from.x, 0, lineLength ), from.y, from.z ) ) - r;
}