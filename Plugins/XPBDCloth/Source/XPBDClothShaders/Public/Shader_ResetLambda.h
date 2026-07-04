

#pragma once

#include "CoreMinimal.h"


class XPBDCLOTHSHADERS_API FResetLambdaShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inNumSprings, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef springsBufferRef);
};
