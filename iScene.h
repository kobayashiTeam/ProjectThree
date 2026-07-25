// IScene.h
#pragma once
#include<d3d11.h>
class Renderer;
class TextRenderer;

class IScene {
public:
    virtual ~IScene() = default;

    virtual bool Enter() { return true; }   // このシーンに入った瞬間の初期化
    virtual void Exit() {}    // このシーンを抜ける瞬間の後片付け

    virtual void Update(float dt) = 0;
    virtual void Submit(Renderer* renderer) = 0;

    // UIテキストの描画。3D描画（Submit→Renderer::Execute）がすべて終わった後、
    // バックバッファに直接重ね描きする。使わないシーンは実装不要（デフォルトで何もしない）
    virtual void SubmitUI(TextRenderer* textRenderer,float dt) {}

    // 「次はこのシーンへ」という遷移要求。無ければnullptrのまま
    virtual IScene* CheckTransition() { return m_nextScene; }

    void SetDevice(ID3D11Device* device) { m_device = device; }

protected:
    ID3D11Device* m_device = nullptr; // 借用。派生クラスからm_deviceとして直接使える
    IScene* m_nextScene = nullptr;
};