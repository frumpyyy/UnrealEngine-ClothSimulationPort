

#pragma once

#include "CoreMinimal.h"

class XPBDCLOTHSHADERS_API FIntegrateShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inNumParticles, FVector3f inGravity, float inDt, FGlobalShaderMap* shaderMap, FRDGBufferRef particlesBufferRef);
};


