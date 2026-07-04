

#pragma once

#include "CoreMinimal.h"

class XPBDCLOTHSHADERS_API FIntegrateShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inNumParticles, uint32 inNumSubsteps, float inDt, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef particlesBufferRef);
};


