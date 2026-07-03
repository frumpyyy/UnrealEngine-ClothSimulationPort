#include "Shader_XPBDCloth.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader_Commons.h"



class FSolveSpringsCS : public FGlobalShader {
public:

	DECLARE_EXPORTED_SHADER_TYPE(FSolveSpringsCS, Global, XPBDCLOTHSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FSolveSpringsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, numSubsteps)
		SHADER_PARAMETER(uint32, springCount)
		SHADER_PARAMETER(uint32, springOffset)
		SHADER_PARAMETER(float, dt)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Particle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Spring>, Springs)
	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}


};


IMPLEMENT_GLOBAL_SHADER(FSolveSpringsCS, "/XPBDCLOTH/SolveSprings.usf", "SolveSprings", SF_Compute);

void FSolveSpringsShaderInterface::AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inSpringOffset, uint32 inSpringCount, uint32 inNumSubsteps, float inDt, FGlobalShaderMap* shaderMap, FRDGBufferRef particlesBufferRef, FRDGBufferRef springsBufferRef) {
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(graphBuilder, "SolveSprings");

	TShaderMapRef<FSolveSpringsCS> cShader(shaderMap);

	FSolveSpringsCS::FParameters* params = graphBuilder.AllocParameters<FSolveSpringsCS::FParameters>();

	params->dt = inDt;
	params->numSubsteps = inNumSubsteps;
	params->springCount = inSpringCount;
	params->springOffset = inSpringOffset;
	params->Particles = graphBuilder.CreateUAV(particlesBufferRef);
	params->Springs = graphBuilder.CreateUAV(springsBufferRef);

	const uint32 threadGroupSize = 256;
	const uint32 particleGroupCount = FMath::DivideAndRoundUp(inSpringCount, threadGroupSize);

	FComputeShaderUtils::AddPass(graphBuilder, RDG_EVENT_NAME("SolveSprings"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, cShader, params, FIntVector(particleGroupCount, 1, 1));

}

