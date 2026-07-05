

#pragma once

#include "CoreMinimal.h"

class XPBDCLOTHSHADERS_API FDebugPointsShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inclothWidth, uint32 inclothHeight, FGlobalShaderMap* shaderMap, FRDGBufferRef debugBufferRef, FRDGTextureRef particleTextureRef);
};
