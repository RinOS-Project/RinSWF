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
- [x] DefineBitsJPEG2/3/4の画像データ分離、JPEG3/4の圧縮アルファ展開
- [x] libjpegが利用可能な環境でのJPEG→RGBA変換、JPEG3/4アルファ合成、BitmapLibraryへの登録
- [x] DefineShape4のLINESTYLE2単色線の線端・結合・スケール・miter情報解析
- [ ] PNG/GIF画像デコーダー統合、JPEG実画像の回帰テスト、LINESTYLE2の塗り線、NewStyles、PlaceObject3、Font
- [ ] ストリーム読み込みとエラー位置の詳細化
- [ ] C ABI（DLL/共有ライブラリ向け）

## Phase 2 — 表示エンジン
- [x] PlaceObject / PlaceObject2、RemoveObject / RemoveObject2、ShowFrameによる基本表示リスト
- [x] PlaceObject / PlaceObject2 のMATRIX（scale/skew/translation）値の保持
- [ ] PlaceObject3、色変換・クリップの保持
- [ ] Shape / Fill / Line / Gradient / Bitmap の表示リスト
- [x] 初期RGBAフレームバッファ、Bitmap転送、線分/二次曲線の簡易ストローク描画
- [x] LINESTYLE幅を反映したクリップ付き簡易太線描画（丸端、4x4被覆率AA）
- [x] FillStyle0/1の辺方向を反映した非ゼロ巻き数スキャンライン塗り（基礎実装）
- [x] 線形・放射状・焦点付きGradientの基本塗り（RGB補間、pad/reflect/repeat、逆MATRIX変換）
- [x] BitmapLibrary参照によるBitmap Fillのタイル・クリップ描画（最近傍とバイリニア）
- [x] 巻き方向が逆の内側輪郭（穴）と画面外Shapeの回帰ケース、スキャンライン範囲の限定
- [x] LINESTYLE2のRound / None（butt）/ Square線端の基本描画
- [x] 連続した同一LINESTYLEの線分に対するRound Joinの基本描画と曲線のCap引数修正
- [x] Bevel Join、Miter JoinとMiterLimit超過時のBevelフォールバック（直線間）
- [x] 線スタイル単位の被覆率マスクによる重複アルファ合成の抑制
- [ ] SWFの輪郭連結・複雑な自己交差形状の検証、LINESTYLE2の線結合精度・端点の連続パス処理、マスクのメモリ使用量最適化
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
