// ScreenBlit.hlsl
// オフスクリーンTextureをバックバッファにそのまま描き写す（初学者向け単純コピー）
//inputLauoutは本来最低限のものでいいが、shader側の実装の都合で全シェーダ共通の
//layoutをつかわなければいけない。ここは仕方なく埋める

// C++（D3D11）側から露出値を制御するための定数バッファ
// ※まだ定数バッファを用意していない場合は、1.0 などの固定値を直書きしても動作します
cbuffer PostProcessConfig : register(b5)
{
    float g_Exposure; // 露出値（基本は 1.0。大きくすると画面が明るくなり、小さくすると暗くなる）
    float3 g_Padding; // 16バイトアライメントのためのパディング
};

struct VS_INPUT
{
    float3 Position : POSITION; // NDC座標 (-1.0 ~ 1.0)
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = float4(input.Position, 1.0);
    output.TexCoord = input.TexCoord;
    return output;
}

Texture2D sceneTexture : register(t0); // オフスクリーンから来たTexture
SamplerState linearSampler : register(s0);

//最初に使っていたオーソドックス
//float4 PS(VS_OUTPUT input) : SV_TARGET
//{
//    float4 color = sceneTexture.Sample(linearSampler, input.TexCoord);
    
//    // 将来ここに軽い調整を入れる余地を残す
//    color.rgb = pow(color.rgb, 1.0 / 2.2); // Gamma修正など
    
//    return color;
//}


//g_exposureを使わないテスト版
//float4 PS(VS_OUTPUT input) : SV_TARGET
//{
//    // 1. テクスチャから色をサンプリング
//    float4 color = sceneTexture.Sample(linearSampler, input.TexCoord);
//    float3 hdrColor = color.rgb;

//    // 2. トーンマッピング（定数バッファを使わず、1.0f でテスト固定）
//    // ★ g_Exposure の代わりに 1.0f を直書きします
//    float3 sdrColor = float3(1.0, 1.0, 1.0) - exp(-hdrColor * 1.0f);

//    // 3. ガンマ補正
//    sdrColor = pow(sdrColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
//    return float4(sdrColor, color.a);
//}

//g_exposureを使う本番版
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    // 1. HDRテクスチャから元の色（1.0超えの可能性あり）をサンプリング
    float4 color = sceneTexture.Sample(linearSampler, input.TexCoord);
    float3 hdrColor = color.rgb;

    // 2. トーンマッピング（露出調整型：Exposure Tone Mapping）
    // 1.0を超えた無限の明るさを、なめらかに 0.0 ～ 1.0 の範囲に収束させます。
    // ※もしC++側からの定数バッファが未実装なら、g_Exposure の代わりに 1.0f などを直書きしてください。
    float3 sdrColor = float3(1.0, 1.0, 1.0) - exp(-hdrColor * g_Exposure); //g_Exposure

    // 【参考】もしもっとシンプルな「Reinhard法」にしたい場合は、上の1行を以下に差し替えてください
    // float3 sdrColor = hdrColor / (hdrColor + float3(1.0, 1.0, 1.0));

    // 3. ガンマ補正（モニター表示用の適切な色空間へ変換）
    // 元のコードの 1.0 / 2.2 をそのまま生かしています
    sdrColor = pow(sdrColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    // アルファ値はそのまま通す（通常は 1.0）
    return float4(sdrColor, color.a);
}

//float4 PS(VS_OUTPUT input) : SV_TARGET
//{
//    // テクスチャサンプリング（一応書いておきますが今回は使いません）
//    float4 color = sceneTexture.Sample(linearSampler, input.TexCoord);

//    // 【検証処理】g_Exposure の値をそのまま RGB に流し込む
//    // もし g_Exposure が 0.0 なら、画面は「真っ黒」になります。
//    // もし g_Exposure が 1.0 なら、画面は「真っ白」になります。
//    // もし g_Exposure が 0.5 なら、画面は「グレー（灰色）」になります。
//    float3 debugColor = float3(g_Exposure, g_Exposure, g_Exposure);

//    return float4(debugColor, 1.0f);
//}