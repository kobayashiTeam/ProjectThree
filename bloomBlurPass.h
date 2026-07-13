#include<d3d11.h>
#include"renderTarget.h"
#include"HorizontalBlurPostProcess.h"
#include"VerticalBlurPostProcess.h"
#include"shaderManager.h"
#include"mesh.h"

class BloomBlurPass {
public:
    // brightRT‚ðŽó‚¯Žæ‚èA‚Ú‚©‚µÏ‚ÝSRV‚ð•Ô‚·‚¾‚¯‚Ì‘¶Ý
    bool Initialize(ID3D11Device* device, UINT width, UINT height) {
        m_hBlur.Initialize(device, ShaderManager::GetInstance().GetShader(ShaderID::HoriBlur));
        m_vBlur.Initialize(device, ShaderManager::GetInstance().GetShader(ShaderID::VerBlur));

        m_tmpRT = new RenderTarget();
        m_tmpRT->Initialize(device, width, height); // ŽÀÛ‚ÌRenderTarget‚ÌAPI‚É‡‚í‚¹‚Ä’²®
        m_pingRT = new RenderTarget();
        m_pingRT->Initialize(device, width, height);
        return true;
    }

    ID3D11ShaderResourceView* Execute(
        ID3D11DeviceContext* ctx, 
        RenderTarget* brightRT,
        Mesh* fullscreenQuad) {

        m_tmpRT->Clear(ctx); m_tmpRT->Bind(ctx);
        m_hBlur.Render(ctx, brightRT);
        fullscreenQuad->Render(ctx);

        m_pingRT->Clear(ctx); m_pingRT->Bind(ctx);
        m_vBlur.Render(ctx, m_tmpRT);
        fullscreenQuad->Render(ctx);

        return m_pingRT->GetSRV();
    }
private:
    RenderTarget* m_tmpRT; RenderTarget* m_pingRT;
    HorizontalBlurPostProcess m_hBlur;
    VerticalBlurPostProcess m_vBlur;
};