

#pragma once

#include "CoreMinimal.h"

class XPBDCLOTHSHADERS_API FNormalsShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inclothWidth, uint32 inclothHeight, uint32 inNumParticles, float inDt, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef particleBufferRef, FRDGBufferRef normalsBufferRef);
};
