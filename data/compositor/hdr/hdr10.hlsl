#include <hdrutils.hlsl>

SamplerState DiffuseSampler{ 
	Filter = MIN_MAG_MIP_LINEAR; 
	AddressU = Wrap;
	AddressV = Wrap;
};
/* Downsample a 2x2 area and convert to greyscale
*/
float4 downscale2x2Luminence(
	float2 uv : TEXCOORD0,
	uniform float2 texelSize, // depends on size of source texture
	uniform Texture2D  inRTT : register(s0)
    ) : SV_TARGET0
{
	
    float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float2 texOffset[4] = {
		-0.5, -0.5,
		-0.5,  0.5, 
		 0.5, -0.5,
		 0.5, 0.5 };

	for( int i = 0; i < 4; i++ )
    {
        // Get colour from source
        accum += inRTT.Sample(DiffuseSampler,uv + texelSize * texOffset[i]);
    }
    
	// Adjust the accumulated amount by lum factor
	// Cannot use float3's here because it generates dependent texture errors because of swizzle
	float lum = dot(accum, LUMINENCE_FACTOR);
	// take average of 4 samples
	lum *= 0.25;
	return lum;

}

/* Downsample a 3x3 area 
 * This shader is used multiple times on different source sizes, so texel size has to be configurable
*/
float4 downscale3x3(
	float2 uv : TEXCOORD0,
	uniform float2 texelSize, // depends on size of source texture
	uniform Texture2D inRTT : register(s0)
    ) : SV_TARGET0
{
	
    float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float2 texOffset[9] = {
		-1.0, -1.0,
		 0.0, -1.0,
		 1.0, -1.0,
		-1.0,  0.0,
		 0.0,  0.0,
		 1.0,  0.0,
		-1.0,  1.0,
		 0.0,  1.0,
		 1.0,  1.0
	};

	for( int i = 0; i < 9; i++ )
    {
        // Get colour from source
        accum += inRTT.Sample(DiffuseSampler, uv + texelSize * texOffset[i]);
    }
    
	// take average of 9 samples
	accum *= 0.1111111111111111;
	return accum;

}

/* Downsample a 3x3 area from main RTT and perform a brightness pass
*/
float4 downscale3x3brightpass(
	float2 uv : TEXCOORD0,
	uniform float2 texelSize, // depends on size of source texture
	uniform Texture2D inRTT : register(s0),
	uniform Texture2D inLum : register(s1)
    ) : SV_TARGET0
{
	
    float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float2 texOffset[9] = {
		-1.0, -1.0,
		 0.0, -1.0,
		 1.0, -1.0,
		-1.0,  0.0,
		 0.0,  0.0,
		 1.0,  0.0,
		-1.0,  1.0,
		 0.0,  1.0,
		 1.0,  1.0
	};

	for( int i = 0; i < 9; i++ )
    {
        // Get colour from source
        accum += inRTT.Sample(DiffuseSampler, uv + texelSize * texOffset[i]);
    }
    
	// take average of 9 samples
	accum *= 0.1111111111111111;

    // Reduce bright and clamp
    accum = max(float4(0.0f, 0.0f, 0.0f, 1.0f), accum - BRIGHT_LIMITER);

	// Sample the luminence texture
	float4 lum = inLum.Sample(DiffuseSampler, float2(0.5f, 0.5f));
	
	// Tone map result
	return toneMap(accum, lum.r);

}

/* Gaussian bloom, requires offsets and weights to be provided externally
*/
float4 bloom(
		float2 uv : TEXCOORD0,
		uniform float2 sampleOffsets[15],
		uniform float4 sampleWeights[15],	
		uniform Texture2D inRTT : register(s0)
		) : SV_TARGET0
{
    float4 accum = float4(0.0f, 0.0f, 0.0f, 1.0f);
	float2 sampleUV;
    
    for( int i = 0; i < 15; i++ )
    {
        // Sample from adjacent points, 7 each side and central
        sampleUV = uv + sampleOffsets[i];
        accum += sampleWeights[i] * inRTT.Sample(DiffuseSampler, sampleUV);
    }
    
    return accum;
	
}
		

/* Final scene composition, with tone mapping
*/
float4 finalToneMapping(
	float2 uv : TEXCOORD0,
	uniform Texture2D inRTT : register(s0),
	uniform Texture2D inBloom : register(s1),
	uniform Texture2D inLum : register(s2)
    ) : SV_TARGET0
{
	// Get main scene colour
    float4 sceneCol = inRTT.Sample(DiffuseSampler, uv);

	// Get luminence value
	float4 lum = inLum.Sample(DiffuseSampler, float2(0.5f, 0.5f));

	// tone map this
	float4 toneMappedSceneCol = toneMap(sceneCol, lum.r);
	
	// Get bloom colour
    float4 bloom = inBloom.Sample(DiffuseSampler, uv);

	// Add scene & bloom
	///  params...
	return float4(toneMappedSceneCol.rgb*1.0 + toneMappedSceneCol.rgb*bloom.rgb*0.8 + 0.3*bloom.rgb, 1.0f);
}


