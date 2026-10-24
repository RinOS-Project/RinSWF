# RinSWF

RinSWFは、SWF (Adobe Flash) ファイルを安全に解析・描画・再生するための、クロスプラットフォームC++20ライブラリです。RinOSと一般的なデスクトップOSでの利用を目指します。

## 現在の実装（v0.1.0 初期段階）

- FWS（非圧縮）とCWS（zlib圧縮）のパース
- SWF RECT（ステージ座標）、フレームレート、フレーム数
- SWFタグヘッダー（短形式・長形式）、ペイロードの抽出
- ファイル・タグごとの上限制御と不正な境界の拒否
- 初期ソフトウェアレンダラー：RGBA Surface、Bitmapのアルファ合成、Shapeの簡易線分・二次曲線ストローク描画（非ゼロ巻き数による単色Fillの基本スキャンライン塗り（描画対象のY範囲だけを走査）に対応、線形・放射状・焦点付きGradientの基本塗りに対応。Bitmap Fillの最近傍・バイリニア補間によるタイル・クリップ描画に対応。LINESTYLEの太線幅と4x4被覆率アンチエイリアシングに基本対応。LINESTYLE2のRound/None/Square線端に基本対応（線分単位）。Round Joinの基本描画を追加。Bevel/Miter JoinとMiterLimitを直線間で基本対応。同一線スタイルで連続・閉路となる辺の余分な線端キャップを抑制。曲線の結合と線スタイル単位の被覆率マスクで線分の重複アルファ合成を抑制（未使用線スタイルのマスク確保を省略しアルファ二重乗算を修正。被覆率マスクを1ch（8-bit）化し4バイト/画素から1バイト/画素へ削減。描画対象を64×64のスパース被覆率タイルで管理し、タイル座標を64-bitキーへパックし、ハッシュテーブルで検索。StrokeLimitsから描画呼び出し全体のタイル数とマスク本体のバイト数上限を設定可能（1タイル4 KiB、管理構造のオーバーヘッドは別）））
- DefineBitsJPEG2/3/4の画像バイト列とアルファチャネル分離・アルファzlib展開（JPEG画像のRGBA変換はlibjpeg利用可能時に対応（PNG/GIFは未対応））
- DefineBitsLossless / DefineBitsLossless2 (パレット、15-bit RGB、32-bit ARGB) のzlib展開とRGBAアセット管理
- 基本タグの型付き解析（SetBackgroundColor、FileAttributes、FrameLabel、SymbolClass、DefineBinaryData）と境界チェック
- PlaceObject / PlaceObject2、RemoveObject / RemoveObject2、ShowFrameによる表示リストのフレームスナップショット生成、MATRIX変換値の保持（描画・ActionScript実行は未対応）

DefineShapeのMove/Line/Quadratic輪郭抽出に加え、DefineShape 1/2/3の単色Fill・LINESTYLEとスタイル番号を解析します。Linear/Radial/Focal Gradientの行列とカラー停止点の解析を追加しました。Bitmap Fill 0x40〜0x43の画像ID・繰り返し/クリップ/スムージング方式・変換行列の解析を追加しました。画像データの展開・描画、DefineShape4のLINESTYLE2単色線の属性解析に対応。塗り線・NewStylesと線端・線結合の正確な描画はまだ未対応です。

**未実装:** ZWS/LZMA、Shape等の複合タグ、描画、ActionScript、音声再生、プレイヤー。現時点ではSWFを実行できません。

## ビルドとテスト

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

zlib開発パッケージ、C++20コンパイラ、Mesonが必要です。JPEGデコードには任意依存のlibjpegが必要です。未導入の場合、JPEGピクセル変換は明示的なエラーになります。

## 利用例

```cpp
#include <rinswf/swf.hpp>
#include <fstream>
#include <iterator>
int main() {
    std::ifstream in("movie.swf", std::ios::binary);
    if (!in) return 1;
    std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>{in}, {});
    auto result = rinswf::parse(bytes);
    if (!result) return 2;
    return result.document.tags.empty();
}
```

## 今後の開発

[TODO.md](TODO.md) に機能別の開発計画を記載しています。

## ライセンス

MIT License — [LICENSE](LICENSE)。既存ライセンスを維持しています。
