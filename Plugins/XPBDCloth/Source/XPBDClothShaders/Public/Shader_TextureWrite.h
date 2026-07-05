

#pragma once

#include "CoreMinimal.h"

class XPBDCLOTHSHADERS_API FTextureWriteShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inclothWidth, uint32 inclothHeight, FGlobalShaderMap* shaderMap, FRDGBufferRef particleBufferRef, FRDGBufferRef normalsBufferRef, FRDGTextureRef posTexture, FRDGTextureRef normalsTexture);
};
