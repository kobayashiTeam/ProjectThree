タイトル:Project Three

ジャンル:技術デモ

<img width="400" height="225" alt="gif1" src="https://github.com/user-attachments/assets/a187a9c4-e74c-421a-8fa7-bbe1003f204f" />


<img width="400" height="225" alt="gif2" src="https://github.com/user-attachments/assets/67ac5d29-d3a1-4eff-913f-e9f819e619fa" />


技術ポイント

(1)遅延レンダリング（Deferred Rendering)：G-Buffer（アルベド／法線／深度）を生成し、Lighting Passで合成。

GBufferの各テクスチャはデバッグ表示として個別に切り替え可能

(2)シャドウマッピング：Directional LightとPoint Light両対応。Point LightはCube Shadow Mapを採用し、

Geometry Shaderで6面を1パスで描画

(3)ノーマルマッピング：法線マップによる陰影表現。ライト方向を動かして陰影の変化を確認できる

(4)HDR + Exposureトーンマッピング：露出値を操作して明るさの見え方を確認

(5)Bloom：MRTとMSAAピンポンバッファによる高輝度部分のにじみ表現

(6)GPUインスタンシング：座標テーブル方式で多数のオブジェクトを効率的に描画

(7)スカイボックス／ガンマ補正：基礎的なレンダリング品質の担保


<br>


工夫ポイント

実装を進める中で、当初1002行あった`renderer.cpp`が肥大化していく課題に直面しました。GBufferPass／SSAOPass／ShadowSystem／PostProcessChain／DeferredLightingPassとして

責務ごとに切り出し、ResourceManagerを導入することで約500行まで整理しました。あわせてScene／GameObject／IScene（Enter/Exit/Update/Submit/CheckTransition）という抽象化を設計し、

各シーンが自分の見せたい機能だけをON/OFFできるアーキテクチャにしています。

「作って終わり」ではなく、後から機能を差し替え・拡張できる設計にすること自体を、このデモを通じて意識しました。

Youtube:

https://youtu.be/_prOC4zQ6fk

参考
[LearnOpenGL](https://learnopengl.com/)（内容をD3D11に翻訳しながら実装）

開発環境
- Visual Studio / DirectX 11
- デバッグ：RenderDoc


