

#pragma once

#include "CoreMinimal.h"


class XPBDCLOTHSHADERS_API FApplyForceShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inNumParticles, FVector3f inGravity, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef particlesBufferRef);
};
