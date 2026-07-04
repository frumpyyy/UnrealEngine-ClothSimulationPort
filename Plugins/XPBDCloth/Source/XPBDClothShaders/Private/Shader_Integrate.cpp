#include "Shader_Integrate.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader_Commons.h"


class FIntegrateCS : public FGlobalShader {
public:

	DECLARE_EXPORTED_SHADER_TYPE(FIntegrateCS, Global, XPBDCLOTHSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FIntegrateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, numParticles)
		SHADER_PARAMETER(uint32, numSubsteps)
		SHADER_PARAMETER(float, dt)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Particle>, Particles)
	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}


};


IMPLEMENT_GLOBAL_SHADER(FIntegrateCS, "/XPBDCLOTH/Integrate.usf", "Integrate", SF_Compute);

void FIntegrateShaderInterface::AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inNumParticles, uint32 inNumSubsteps, float inDt, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef particlesBufferRef) {
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(graphBuilder, "Integrate");

	TShaderMapRef<FIntegrateCS> cShader(shaderMap);

	FIntegrateCS::FParameters* params = graphBuilder.AllocParameters<FIntegrateCS::FParameters>();

	params->dt = inDt;
	params->numSubsteps = inNumSubsteps;
	params->numParticles = inNumParticles;
	params->Particles = graphBuilder.CreateUAV(particlesBufferRef);

	FComputeShaderUtils::AddPass(graphBuilder, RDG_EVENT_NAME("Integrate"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, cShader, params, FIntVector(groupSize, 1, 1));

}

