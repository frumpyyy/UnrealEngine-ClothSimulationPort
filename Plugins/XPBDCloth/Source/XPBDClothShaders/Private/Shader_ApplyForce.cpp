#include "Shader_ApplyForce.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader_Commons.h"

class FApplyForceCS : public FGlobalShader {
public:

	DECLARE_EXPORTED_SHADER_TYPE(FApplyForceCS, Global, XPBDCLOTHSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FApplyForceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, numParticles)
		SHADER_PARAMETER(FVector3f, gravity)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Particle>, Particles)
	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}


};


IMPLEMENT_GLOBAL_SHADER(FApplyForceCS, "/XPBDCLOTH/ApplyForces.usf", "ApplyForces", SF_Compute);

void FApplyForceShaderInterface::AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inNumParticles, FVector3f inGravity, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef particlesBufferRef) {

	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(graphBuilder, "ApplyForces");

	TShaderMapRef<FApplyForceCS> cShader(shaderMap);

	FApplyForceCS::FParameters* params = graphBuilder.AllocParameters<FApplyForceCS::FParameters>();

	params->Particles = graphBuilder.CreateUAV(particlesBufferRef);
	params->numParticles = inNumParticles;
	params->gravity = inGravity;

	FComputeShaderUtils::AddPass(graphBuilder, RDG_EVENT_NAME("ApplyForces"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, cShader, params, FIntVector(groupSize, 1, 1));

}

