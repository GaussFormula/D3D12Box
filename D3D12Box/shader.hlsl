cbuffer cbPerObject:register(b0)
{
    float4x4 gWorldViewProj;
};

struct VSInput
{
    float3 posL:POSITION;
    float4 color:COLOR;
};

struct PSInput
{
    float4 posH:SV_POSITION;
    float4 color:COLOR;
};

PSInput VSMain(VSInput vin)
{
    PSInput result;

    // Transform to homogeneous clip spaces.
    result.posH = mul(float4(vin.posL, 1.0f), gWorldViewProj);
    result.color = vin.color;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}