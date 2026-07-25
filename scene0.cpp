#include"scene0.h"
#include <DirectXPackedVector.h> // 必要に応じてインクルード 新規

#include"litMaterial.h"
#include"unLitMaterial.h"
#include"normalVizMaterial.h"
#include"normalMappingMaterial.h"
#include"parallaxMappingMaterial.h"
#include"deferredCBMaterial.h"
#include"outLineMaterial.h"
#include"mesh.h"
#include"resourceManager.h"
#include"outLine.h"

#include"moveGSEffect.h"
#include"normalVizGSEffect.h"

#include"shaderManager.h"

bool Scene0::Enter(){
	return true;
}

