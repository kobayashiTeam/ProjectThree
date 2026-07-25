// textRenderer.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>

// 前方宣言ではなく、ここで直接includeする。
// DirectXTKは2022年9月リリース以降、SpriteBatch等を
// namespace DirectX { inline namespace DX11 { ... } } の中で定義しており、
// 見かけ上はDirectX::SpriteBatchでも実体はDirectX::DX11::SpriteBatch。
// ここで独自にDirectX直下へ前方宣言すると、本物のヘッダをincludeした
// 翻訳単位で「同名だが別実体」の2つが見えてあいまい(ambiguous)になるため、
// 素直にヘッダを取り込む方式にしている。
#include <directxtk/SpriteBatch.h>
#include <directxtk/SpriteFont.h>
#include <directxtk/CommonStates.h>


// UIテキスト専用の描画役。
// 3D描画（Renderer）とは別ルートで、Game::Render()の最後に
// バックバッファへ直接重ね描きする想定。
class TextRenderer
{
public:
    TextRenderer() = default;
    ~TextRenderer();

    // fontFilePath: MakeSpriteFont.exe等で事前に作った .spritefont ファイルへのパス
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* fontFilePath);

    // 呼び出し順は必ず Begin() -> DrawString()を好きなだけ -> End()
    // 3D側の描画（Renderer::Execute()）がすべて終わり、
    // バックバッファがレンダーターゲットにセットされた状態で呼ぶこと
    void Begin();
    void DrawString(const wchar_t* text, float x, float y,
        float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f,
        float scale = 1.0f);
    void End();

    // 右寄せ・中央寄せなどのレイアウト計算に使う、文字列の描画サイズ
    void MeasureString(const wchar_t* text, float& outWidth, float& outHeight) const;

    bool IsReady() const { return m_spriteFont != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    std::unique_ptr<DirectX::SpriteBatch>  m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>   m_spriteFont;
    std::unique_ptr<DirectX::CommonStates> m_states;
};