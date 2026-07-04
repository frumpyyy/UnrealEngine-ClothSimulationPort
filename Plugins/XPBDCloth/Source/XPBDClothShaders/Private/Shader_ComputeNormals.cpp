#include "Shader_ComputeNormals.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader_Commons.h"


class FNormalsCS : public FGlobalShader {
public:

	DECLARE_EXPORTED_SHADER_TYPE(FNormalsCS, Global, XPBDCLOTHSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FNormalsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, clothWidth)
		SHADER_PARAMETER(uint32, clothHeight)
		SHADER_PARAMETER(uint32, numParticles)
		SHADER_PARAMETER(float, dt)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Particle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float3>, OutNormals)
	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}


};


IMPLEMENT_GLOBAL_SHADER(FNormalsCS, "/XPBDCLOTH/ComputeNormals.usf", "ComputeNormals", SF_Compute);

void FNormalsShaderInterface::AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inclothWidth, uint32 inclothHeight, uint32 inNumParticles, float inDt, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef particleBufferRef, FRDGBufferRef normalsBufferRef) {
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(graphBuilder, "ComputeNormals");

	TShaderMapRef<FNormalsCS> cShader(shaderMap);

	FNormalsCS::FParameters* params = graphBuilder.AllocParameters<FNormalsCS::FParameters>();

	params->clothWidth = inclothWidth;
	params->clothHeight = inclothHeight;
	params->numParticles = inNumParticles;
	params->dt = inDt;
	params->Particles = graphBuilder.CreateUAV(particleBufferRef);
	params->OutNormals = graphBuilder.CreateUAV(normalsBufferRef);

	FComputeShaderUtils::AddPass(graphBuilder, RDG_EVENT_NAME("ComputeNormals"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, cShader, params, FIntVector(groupSize, 1, 1));

}
