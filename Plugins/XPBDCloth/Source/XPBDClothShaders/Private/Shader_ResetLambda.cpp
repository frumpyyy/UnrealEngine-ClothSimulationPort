


#include "Shader_ResetLambda.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader_Commons.h"





class FResetLambdaCS : public FGlobalShader {
public:

	DECLARE_EXPORTED_SHADER_TYPE(FResetLambdaCS, Global, XPBDCLOTHSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FResetLambdaCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, numSprings)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Spring>, Springs)
	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}


};


IMPLEMENT_GLOBAL_SHADER(FResetLambdaCS, "/XPBDCLOTH/ResetLambda.usf", "ResetLambda", SF_Compute);

void FResetLambdaShaderInterface::AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inNumSprings, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef springsBufferRef) {
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(graphBuilder, "ResetLambda");

	TShaderMapRef<FResetLambdaCS> cShader(shaderMap);

	FResetLambdaCS::FParameters* params = graphBuilder.AllocParameters<FResetLambdaCS::FParameters>();

	params->Springs = graphBuilder.CreateUAV(springsBufferRef);
	params->numSprings = inNumSprings;

	FComputeShaderUtils::AddPass(graphBuilder, RDG_EVENT_NAME("ResetLambda"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, cShader, params, FIntVector(groupSize, 1, 1));

}

