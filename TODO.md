# RinSWF 開発計画

## Phase 1 — パーサーと安全な基盤
- [x] C++20 / Meson のビルド定義
- [x] 公開C++ APIと独立ライブラリ
- [x] FWS / CWS ヘッダーと zlib 展開
- [x] RECT、フレーム情報、タグ列の解析
- [x] 入力・展開・タグ数・タグ長の制限
- [ ] ZWS（LZMA）対応
- [x] 基本タグの型付きデコード（SetBackgroundColor、FileAttributes、FrameLabel、SymbolClass、DefineBinaryData）
- [x] DefineShape 1/2/3/4のRECT・基本edgeレコード解析（スタイルなし限定）
- [x] DefineShape 1/2/3 のSolid FillとLINESTYLE、スタイル参照
- [x] Linear / Radial / Focal Gradientの解析（行列・停止点・補間情報）
- [x] Bitmap Fill 0x40–0x43 の参照IDと変換行列の解析
- [x] DefineBitsLossless / DefineBitsLossless2のRGB(A)ピクセル展開とBitmapLibrary ID検索
- [ ] DefineBitsJPEG系列、LINESTYLE2、NewStyles、PlaceObject3、Font
- [ ] ストリーム読み込みとエラー位置の詳細化
- [ ] C ABI（DLL/共有ライブラリ向け）

## Phase 2 — 表示エンジン
- [x] PlaceObject / PlaceObject2、RemoveObject / RemoveObject2、ShowFrameによる基本表示リスト
- [x] PlaceObject / PlaceObject2 のMATRIX（scale/skew/translation）値の保持
- [ ] PlaceObject3、色変換・クリップの保持
- [ ] Shape / Fill / Line / Gradient / Bitmap の表示リスト
- [ ] ベクターラスタライズとクリッピング
- [ ] テキスト、フォント、変換行列
- [ ] バックエンド抽象化（RinOS、Linux、Windows）

## Phase 3 — 実行環境
- [ ] Timeline、Sprite、DisplayObject
- [ ] イベント・入力・音声
- [ ] AVM1 / ActionScript 1・2
- [ ] AVM2 / ActionScript 3 とABC
- [ ] セキュリティサンドボックス、ネットワーク権限制御

## Phase 4 — 統合と互換性
- [ ] スタンドアロンSWFプレイヤー
- [ ] RinOS WebEngine向け埋め込みAPI
- [ ] SVG、動画などへの変換
- [ ] 互換性テストとファジング

方針: レガシーSWF互換実装であり、新規Web標準やRinOS標準UI形式にはしない。推測による成功扱いや危険な権限昇格を設けない。
