// =========================================================
// 色反転のCS版（PS版：InversionShader.hlsl と同じ結果になる）
//
// 1スレッド ＝ 1ピクセル。8x8 = 64スレッドを1グループとし、
// C++側が画面を覆うだけのグループ数を Dispatch で投げる
// =========================================================

// 1グループの縦横のスレッド数。InversionCSPostProcess::THREAD_GROUP_SIZE と必ずそろえる
#define THREAD_GROUP_SIZE 8

// 読み込み：直前の段の絵（SRV）
Texture2D<float4> g_input : register(t0);
// 書き込み：この段の出力（UAV）。RWは「読み書きできる」の意味。ここでは書くだけ
RWTexture2D<float4> g_output : register(u0);

// C++の InversionCSPostProcess::PerEffectCB と同じ並び
cbuffer InversionCB : register(b0)
{
    float g_intensity; // 0.0 = 元の色, 1.0 = 完全に反転
    uint2 g_size; // 書き込み先の大きさ
    float g_pad;
};

// SV_DispatchThreadID：Dispatch全体の中での、このスレッドの通し番号（x, y）
//   ＝ グループ番号 × THREAD_GROUP_SIZE ＋ グループの中での番号
//   1スレッド＝1ピクセルなので、そのままピクセルの座標として使える
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // グループ数を切り上げたぶん、画面の外にはみ出したスレッドは何もしない
    if (id.x >= g_size.x || id.y >= g_size.y)
        return;

    // サンプラーを使わず、ピクセルの座標で直接読む（Load と同じ。補間なし）
    float4 color = g_input[id.xy];
    float3 inverted = 1.0f - color.rgb;
    g_output[id.xy] = float4(lerp(color.rgb, inverted, g_intensity), color.a);
}
