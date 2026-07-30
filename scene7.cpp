#include"scene7.h"

bool Scene7::Enter() {
    // GPUインスタンシングの立方体群はRenderer側のInstancedModelが直接描画するため、
    // 他シーンのようなGameObject/Modelの登録は不要
    return true;
}