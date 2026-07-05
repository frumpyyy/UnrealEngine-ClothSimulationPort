


#include "Shader_TextureWrite.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader_Commons.h"

class FWriteTexturesLambdaCS : public FGlobalShader {
public:

	DECLARE_EXPORTED_SHADER_TYPE(FWriteTexturesLambdaCS, Global, XPBDCLOTHSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FWriteTexturesLambdaCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, clothWidth)
		SHADER_PARAMETER(uint32, clothHeight)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Particle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, Normals)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4f>, PositionOut)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4f>, NormalsOut)
	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}


};


IMPLEMENT_GLOBAL_SHADER(FWriteTexturesLambdaCS, "/XPBDCLOTH/WriteTextures.usf", "WriteTextures", SF_Compute);


void FTextureWriteShaderInterface::AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inclothWidth, uint32 inclothHeight, FGlobalShaderMap* shaderMap, FRDGBufferRef particleBufferRef, FRDGBufferRef normalsBufferRef,
	FRDGTextureRef posTexture, FRDGTextureRef normalsTexture) {
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(graphBuilder, "WriteTextures");

	TShaderMapRef<FWriteTexturesLambdaCS> cShader(shaderMap);

	FWriteTexturesLambdaCS::FParameters* params = graphBuilder.AllocParameters<FWriteTexturesLambdaCS::FParameters>();

	params->clothWidth = inclothWidth;
	params->clothHeight = inclothHeight;
	params->Particles = graphBuilder.CreateUAV(particleBufferRef);
	params->Normals = graphBuilder.CreateUAV(normalsBufferRef);
	params->PositionOut = graphBuilder.CreateUAV(posTexture);
	params->NormalsOut = graphBuilder.CreateUAV(normalsTexture);

	int xGroup = FMath::DivideAndRoundUp(inclothWidth, 8u);
	int yGroup = FMath::DivideAndRoundUp(inclothHeight, 8u);

	FComputeShaderUtils::AddPass(graphBuilder, RDG_EVENT_NAME("WriteTextures"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, cShader, params, FIntVector(xGroup, yGroup, 1));

}
