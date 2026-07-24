// textRenderer.cpp
#include "textRenderer.h"
//#include <directxtk/SpriteBatch.h>
//#include <directxtk/SpriteFont.h>
//#include <directxtk/CommonStates.h>

TextRenderer::~TextRenderer() = default;

bool TextRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* fontFilePath)
{
    m_context = context;

    m_states = std::make_unique<DirectX::CommonStates>(device);
    m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(context);

    try
    {
        m_spriteFont = std::make_unique<DirectX::SpriteFont>(device, fontFilePath);
    }
    catch (...)
    {
        // .spritefontが見つからない/壊れている場合、DirectXTKは例外を投げてくる
        m_spriteFont.reset();
        return false;
    }

    return true;
}

void TextRenderer::Begin()
{
    // 3D用に組んであるパイプラインステート（深度テスト・カリング等）を、
    // SpriteBatchが要求する2D向けステートに一時的に差し替える。
    // Renderer::Execute()は毎フレーム、各パスで使うステートを明示的に
    // 再バインドしているため、ここで状態を崩しても次フレームには影響しない。
    m_spriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        m_states->NonPremultiplied(),
        nullptr,                 // サンプラー：デフォルト(Linear Clamp相当)を使う
        m_states->DepthNone(),   // UIは深度テストなしで常に最前面に描く
        m_states->CullNone()
    );
}

void TextRenderer::DrawString(const wchar_t* text, float x, float y,
    float r, float g, float b, float a, float scale)
{
    if (!m_spriteFont) return;

    DirectX::XMVECTORF32 color = { r, g, b, a };
    m_spriteFont->DrawString(
        m_spriteBatch.get(),
        text,
        DirectX::XMFLOAT2(x, y),
        color,
        0.0f,                           // 回転なし
        DirectX::XMFLOAT2(0.0f, 0.0f),  // 原点：左上基準
        scale
    );
}

void TextRenderer::End()
{
    m_spriteBatch->End();
}

void TextRenderer::MeasureString(const wchar_t* text, float& outWidth, float& outHeight) const
{
    if (!m_spriteFont)
    {
        outWidth = 0.0f;
        outHeight = 0.0f;
        return;
    }

    DirectX::XMVECTOR size = m_spriteFont->MeasureString(text);
    outWidth = DirectX::XMVectorGetX(size);
    outHeight = DirectX::XMVectorGetY(size);
}