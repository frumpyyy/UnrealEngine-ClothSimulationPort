#include "Shader_DebugPoints.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader_Commons.h"





class FDebugPointsCS : public FGlobalShader {
public:

	DECLARE_EXPORTED_SHADER_TYPE(FDebugPointsCS, Global, XPBDCLOTHSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FDebugPointsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, clothWidth)
		SHADER_PARAMETER(uint32, clothHeight)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float3>, DebugOut)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4f>, PositionIn)
	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}


};


IMPLEMENT_GLOBAL_SHADER(FDebugPointsCS, "/XPBDCLOTH/DebugPoints.usf", "DebugPoints", SF_Compute);

void FDebugPointsShaderInterface::AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inclothWidth, uint32 inclothHeight, FGlobalShaderMap* shaderMap, FRDGBufferRef debugBufferRef, FRDGTextureRef particleTextureRef) {
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(graphBuilder, "DebugPoints");

	TShaderMapRef<FDebugPointsCS> cShader(shaderMap);

	FDebugPointsCS::FParameters* params = graphBuilder.AllocParameters<FDebugPointsCS::FParameters>();

	params->clothHeight = inclothHeight;
	params->clothWidth = inclothWidth;
	params->PositionIn = graphBuilder.CreateUAV(particleTextureRef);
	params->DebugOut = graphBuilder.CreateUAV(debugBufferRef);

	const uint32 debugGroupX = FMath::DivideAndRoundUp(inclothWidth, 8u);
	const uint32 debugGroupY = FMath::DivideAndRoundUp(inclothHeight, 8u);

	FComputeShaderUtils::AddPass(graphBuilder, RDG_EVENT_NAME("DebugPoints"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, cShader, params, FIntVector(debugGroupX, debugGroupY, 1));

}

