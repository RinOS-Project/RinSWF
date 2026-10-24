# RinSWF

RinSWFは、SWF (Adobe Flash) ファイルを安全に解析・描画・再生するための、クロスプラットフォームC++20ライブラリです。RinOSと一般的なデスクトップOSでの利用を目指します。

## 現在の実装（v0.1.0 初期段階）

- FWS（非圧縮）とCWS（zlib圧縮）のパース
- SWF RECT（ステージ座標）、フレームレート、フレーム数
- SWFタグヘッダー（短形式・長形式）、ペイロードの抽出
- ファイル・タグごとの上限制御と不正な境界の拒否
- 基本タグの型付き解析（SetBackgroundColor、FileAttributes、FrameLabel、SymbolClass、DefineBinaryData）と境界チェック
- PlaceObject / PlaceObject2、RemoveObject / RemoveObject2、ShowFrameによる表示リストのフレームスナップショット生成、MATRIX変換値の保持（描画・ActionScript実行は未対応）

**未実装:** ZWS/LZMA、Shape等の複合タグ、描画、ActionScript、音声再生、プレイヤー。現時点ではSWFを実行できません。

## ビルドとテスト

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

zlib開発パッケージ、C++20コンパイラ、Mesonが必要です。

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
