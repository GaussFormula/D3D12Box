cbuffer cbPerObject:register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerPass:register(b1)
{
    float4x4    gView;
    float4x4    gInvView;
    float4x4    gProj;
    float4x4    gInvProj;
    float4x4    gViewProj;
    float4x4    gInvViewProj;
    float3      gEyePosW;
    float       cbPerObjectPad1;
    float2      gRenderTargerSize;
    float2      gInvRenderTargetSize;
    float2      gNearZ;
    float2      gFarZ;
    float       gTotalTime;
    float       gDeltaTime;
}

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
    float4 posW = mul(float4(vin.posL, 1.0f), gWorld);
    result.posH = mul(posW, gWorldViewProj);
    result.color = vin.color;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}