# RinSWF 開発計画

## Phase 1 — パーサーと安全な基盤
- [x] C++20 / Meson のビルド定義
- [x] 公開C++ APIと独立ライブラリ
- [x] FWS / CWS ヘッダーと zlib 展開
- [x] RECT、フレーム情報、タグ列の解析
- [x] 入力・展開・タグ数・タグ長の制限
- [x] ZWS（LZMA）圧縮の解析・展開（任意liblzma依存、圧縮長・展開長・辞書サイズ・SWFバージョン検査、デコーダー解放処理を実装。ビルド・実SWF検証は未実施）
- [x] 基本タグの型付きデコード（SetBackgroundColor、FileAttributes、FrameLabel、SymbolClass、DefineBinaryData）
- [x] DefineShape 1/2/3/4のRECT・基本edgeレコード解析（スタイルなし限定）
- [x] DefineShape 1/2/3 のSolid FillとLINESTYLE、スタイル参照
- [x] Linear / Radial / Focal Gradientの解析（行列・停止点・補間情報）
- [x] Bitmap Fill 0x40–0x43 の参照IDと変換行列の解析
- [x] DefineBitsLossless / DefineBitsLossless2のRGB(A)ピクセル展開とBitmapLibrary ID検索
- [x] DefineBitsJPEG2/3/4の画像データ分離、JPEG3/4の圧縮アルファ展開
- [x] libjpegが利用可能な環境でのJPEG→RGBA変換、JPEG3/4アルファ合成、BitmapLibraryへの登録
- [x] DefineShape4のLINESTYLE2単色線の線端・結合・スケール・miter情報解析
- [ ] PNG/GIF画像デコーダー統合、JPEG実画像の回帰テスト、PlaceObject3、Font
- [ ] ストリーム読み込みとエラー位置の詳細化
- [x] C ABI（DLL/共有ライブラリ向け）：不透明Document所有権、FWS/CWS解析、リソース上限、ヘッダー・タグ参照、C向けステータスを実装（ビルド・動作検証未実施。Movie生成・フレームRGBA描画APIも追加済み。ビルド・動作検証未実施）

## Phase 2 — 表示エンジン
- [x] PlaceObject / PlaceObject2、RemoveObject / RemoveObject2、ShowFrameによる基本表示リスト
- [x] PlaceObject / PlaceObject2 のMATRIX（scale/skew/translation）値の保持
- [x] PlaceObject（タグ4）のRGB CXFORM（乗算・加算）を解析し、既存の独立レイヤー色変換描画へ統合（Alphaチャンネルは維持。ビルド・回帰未検証）
- [ ] PlaceObject3の拡張フィールド、クリップや拡張描画属性の保持（PlaceObject3の基本配置・移動・置換・MATRIX・名前・ratioおよびCXFORMWITHALPHAの解析・描画合成は実装済み。一部Filter/BlendModeは実装済み。ClipDepthアルファマスクも基本対応、複雑なマスク合成や非Normal Blendとの併用は未対応）
- [ ] Shape / Fill / Line / Gradient / Bitmap の表示リスト
- [x] FrameSnapshotの深度順オブジェクトからShapeとLossless BitmapをRGBAサーフェスへ描画する基本API（平行移動限定）
- [x] フレーム描画アセット収集へJPEG2/3/4を追加し、重複character IDを拒否
- [x] Bitmap配置のアフィン逆変換（拡大縮小・回転・スキュー・サブピクセル平行移動、最近傍）
- [x] Shapeの拡大縮小・回転・スキューをローカルRGBA描画後のアフィン変換で基本対応（16M画素上限）
- [x] アフィンBitmap/Shape描画にプリマルチプライドアルファのバイリニア補間を導入
- [x] 単色Fill・線なしShapeの輪郭座標を直接アフィン変換して描画（整数twipsに丸め）
- [x] 等方拡大縮小・回転の単色ストロークを線幅と輪郭座標の直接変換で描画
- [x] 単一直線・buttキャップのストロークを矩形輪郭として生成し、非等方アフィン変換を直接描画
- [x] 不透明な複数直線・Buttキャップの非等方ストロークを変換矩形輪郭として描画
- [x] 非等方変換された不透明複数直線のBevel Join用三角形輪郭を追加
- [x] 不透明直線のMiter JoinとMiterLimit超過時のBevelフォールバックを非等方変換経路へ追加
- [x] 非等方変換された不透明複数直線のRound Joinを円弧扇形の三角形群で基本描画
- [x] 非等方変換された半透明ストロークの輪郭を一括ラスタライズし、アルファを一度だけ合成
- [x] 変換ストロークの各輪郭を個別に描画し、被覆率の最大値でUnion（逆方向の重複輪郭による巻き数相殺を回避）
- [x] 輪郭ごとの一時サーフェスを描画矩形内に限定し、全画面クリアを削減
- [x] 非等方ストロークUnionの全画面マスクを輪郭全体のクリップ矩形へ縮小
- [x] Union被覆率マスクをRGBAから8-bit単一チャンネルへ変更
- [x] Union被覆率マスクを64×64のスパースタイルに変更し、4096タイル上限を設定
- [x] 二次ベジェ曲線の曲率に応じた線分細分化を非等方ストローク輪郭描画に統合
- [x] 細分化の画面空間誤差上限を明確化し、過大曲率・非有限値・座標オーバーフローを拒否
- [x] 曲線細分化で重複座標を除外し、ほぼ一直線の接合部で不要なJoin楔を省略
- [x] 閉じた細分化パスの最後・最初の辺にRound / Bevel / Miter Joinを適用
- [x] 明示的な閉路ではSquare Capの延長・Round Capの半円生成を抑制
- [x] Union描画の輪郭ごとのスクラッチサーフェスを同一寸法で再利用
- [x] 退化輪郭の描画省略と隣接ピクセルでのタイル検索キャッシュを追加
- [x] unordered_map再ハッシュ時のイテレーター失効を避ける安定ポインタのキャッシュへ変更
- [x] 非等方Unionマスクのタイル上限を固定値から公開StrokeLimitsの共有設定に統一
- [x] アフィンストロークのUnionを4×4サブピクセルbitmaskのOR合成へ変更
- [x] サンプルごとの輪郭座標変換を事前計算し、交差しない辺をY範囲で除外
- [x] 各スキャンラインの4サンプル行ごとに有効辺リストを生成してX方向の判定に再利用
- [x] サブピクセル行の辺開始・終了イベントでAETを増分更新し、全辺の再走査を省略
- [x] AET交点のX座標ソートと巻き数の後方累積による高速サンプル判定
- [x] 交点数24以下は挿入ソート、超過時はstd::sortへ切り替えるハイブリッド処理
- [x] 隣接サンプル行でAETの辺順序を保持し、追加・削除と交差に応じて並べ替え
- [x] 前サンプル行のX交点から辺の傾きに基づいて増分更新し、新規辺のみ直接算出
- [x] AETの前行インデックスと有効辺所属判定をハッシュ管理し、繰り返し線形探索を削減
- [x] AETの作業用ベクター・集合・交点マップをサブピクセル行間で再利用
- [x] AETの増分X交点を64行ごとに再計算し、浮動小数点の誤差蓄積を抑制
- [x] AETの辺開始・終了イベントを疎なハッシュマップに保持し、高さ比例の巨大配列確保を回避
- [x] 完全に画面外のアフィンストロークはマスク上限が0でも成功させ、実際のタイル確保時に上限を適用
- [x] サンプル行の交点X最小・最大範囲でピクセル走査をクリップし、交点のない行をスキップ
- [ ] 同一輪郭内の自己交差・閉路・曲線の精度とUnion描画性能、およびGradient/Bitmap Fillの直接変換・色変換・Sprite対応
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
- [x] 線分・曲線をマスクに統一、被覆率とソースアルファの二重乗算を修正、未使用線スタイルのマスク確保を省略
- [x] ストローク被覆率マスクをRGBA（4バイト/画素）から8-bit単一チャンネル（1バイト/画素）へ変更
- [x] 線スタイルごとの描画範囲を推定し、画面内の矩形領域だけ被覆率マスクを確保
- [x] スパース64×64タイルマスクに移行、タイル数上限と離れた線分の回帰ケースを追加
- [x] StrokeLimitsによる描画呼び出し全体のタイル数・マスク本体バイト数の予算設定と超過時のfail-closed
- [x] スパース被覆率タイルの探索をstd::mapから座標パックしたstd::unordered_mapへ変更
- [x] 同一LINESTYLEの連続辺で内部キャップを抑制し、明示的に閉じた輪郭の外部キャップを抑制
- [x] 閉路の最終辺から最初の辺へのRound / Bevel / Miter Join接続（直線パス限定）
- [x] 閉路の曲線端点の接線方向を用いた最終辺から最初の辺へのJoinの基本対応
- [x] 連続する二次ベジェ曲線・直線間のJoinを端点接線から計算（縮退接線は弦へフォールバック）
- [x] 連続する二次曲線間のJoinに端点接線を使用
- [x] スタイルなしの辺をまたぐ接線連続性を解除し、スタイル切替時の独立描画ケースを追加
- [x] DefineShape4のLINESTYLE2 Bitmap塗り線を解析し、BitmapLibraryの反復・クリップ・補間サンプリングをストロークへ統合（欠落Bitmapは明示エラー、ビルド・描画未検証）
- [x] DefineShape4のLINESTYLE2グラデーション塗り線を解析し、被覆率マスクに線形・放射状・焦点グラデーションをサンプリング（Bitmap塗り線未対応、ビルド・描画未検証）
- [x] DefineShape4のLINESTYLE2でHasFillFlag=1の単色RGBA塗り線を解析し、既存ストローク描画へ統合（グラデーション・Bitmap塗り線は未対応、ビルド未検証）
- [x] DefineShape2/3/4のStateNewStylesを解析し、追加Fill/Line配列のローカルスタイル番号をグローバル参照へ変換（ビルド・実SWF検証は未実施）
- [ ] SWFの輪郭連結・複雑な自己交差形状の検証、異なるスタイル間の連結、タイル上限の実測ベンチマーク・性能検証
- [ ] DefineEditText（タグ37）のフィールド解析・アセット登録と、埋め込みDefineFont2/3の文字コード・advance表を用いた単一行プレーン初期文字列の描画経路を追加（UTF-8のBMP範囲に限定、明示改行CR/LF/CRLFとline-height適用を追加。文字単位の簡易自動折返しを追加（ASCII単語の幅を先読みして行内に収まる場合は単語単位で折返し（長い単語は文字単位にフォールバック）。非ASCIIの単語分割・行単位の中央・右揃えを追加。配置行列の逆写像によるEditText矩形クリップを追加（画素中心判定、Shapeラスタライズ時に領域外画素をスキップしフルRGBA画面退避を削除）。均等割付・HTML・編集UI・AVM連携と境界AAの精密化は未実装）。ビルド未検証）
- [ ] テキスト、フォント、変換行列（DefineFont（タグ10）およびDefineFont2/3（タグ48/75）のoffset/code/layout表・SHAPE輪郭解析と重複ID検査を追加。DefineFont3は20倍座標単位を補正。DefineText/DefineText2のGlyphEntry、色、サイズ、位置とFont輪郭描画を追加（ビルド・実SWF表示検証未実施））
- [ ] バックエンド抽象化（RinOS、Linux、Windows）

## Phase 3 — 実行環境
- [ ] Timeline、Sprite、DisplayObject（DefineSprite内のタグ列解析・独立FrameSnapshot構築・Sprite初期フレームの再帰描画と親子行列合成は実装済み。親のフレーム番号指定によるSpriteフレーム選択・ループは実装済み。インスタンスの配置フレーム起点の進行とキャラクター置換時のリセットは実装済み。停止・再開・ActionScript制御と親タイムラインのループ時の再生成は未実装）
- [ ] イベント・入力・音声
- [ ] AVM1 / ActionScript 1・2（独立・ホスト権限なしのPush数値/Pop/算術/比較/Jump/If/End実行器（ジャンプ先命令境界と実行回数を検証）を追加。独立typed VMに文字列・Boolean・Null/Undefined相当値とGetVariable/SetVariable、条件分岐を追加（変数数制限・成功時だけ反映）。DoActionタグをShowFrameごとに保持し明示的なexecute_frame_actionsで順序実行（失敗時の変数ロールバック）。自動実行・Timeline制御・関数・スコープチェーン・イベント・DisplayObject操作は未実装。ビルド未検証）
- [ ] AVM2 / ActionScript 3 とABC
- [ ] セキュリティサンドボックス、ネットワーク権限制御

## Phase 4 — 統合と互換性
- [ ] スタンドアロンSWFプレイヤー
- [ ] RinOS WebEngine向け埋め込みAPI
- [ ] SVG、動画などへの変換
- [ ] 互換性テストとファジング

方針: レガシーSWF互換実装であり、新規Web標準やRinOS標準UI形式にはしない。推測による成功扱いや危険な権限昇格を設けない。

- [x] アフィンストロークのX走査範囲を実交点の最小・最大値から算出し、画面端の部分クリッピング比較テストを追加
- [ ] 画面右端・上端・下端と複数輪郭をまたぐストロークの描画一致テストを実行し結果を確認
- [ ] 負座標を含むストロークを平行移動した大きい描画領域と比較する回帰テストを実行・検証
- [x] アフィンストロークのXサンプル判定を交点カーソルの単調走査に変更し、画素ごとの二分探索を削減
- [x] AETの有効辺がないサブピクセル行は交点再整列・ハッシュ構築を省略し、前行の状態をリセット
- [x] スキャンライン間で4サンプル行分の有効辺・交点・巻き数バッファを再利用し、繰り返し確保を削減
- [x] AET有効辺をインデックス付き配列で管理し、終了イベントでの線形検索・eraseシフトを削減
- [x] 4サンプル行の有効辺スナップショットコピーを廃止し、イベント更新直後のAETを直接処理
- [x] AET交点の辺とX座標をペアでソートし、一時Xハッシュマップとソート後X配列の重複生成を削減
- [x] AETの前行の辺順序と交点X座標を単一のペア配列へ統合し、ソート後の二重コピーを削減
- [x] AETのソート済み交点から直接次行の交点を生成し、前行インデックスハッシュと辺ポインタ作業配列を削除
- [x] AET交点が4×4サンプル境界に近い場合に元の辺から再計算し、累積誤差による被覆の反転を抑制
- [ ] 4×4サンプル位置（x=2.125px）を通る異方性ストロークの平行移動比較テストを実行し、被覆差分を確認
- [x] AETの交点X座標をstd::fmaで計算し、増分更新・再アンカー時の中間丸めを削減
- [x] 画面外AET交点のサブピクセル位相チェックを省略し、可視領域周辺のみ境界補正を行う
- [x] TimelinePlaybackで親タイムラインの外部クロック・pause/resume/seek/advanceとrender_playback APIを追加
- [x] TimelineResultの親フレーム切替・ループ選択・ループ起点のSprite再生をrender_timeline_playbackで追加（独立再生状態とActionScriptは未実装）
- [x] SpritePlaybackにroot depth別のstop/play/seek/advance/eraseと独立描画オーバーロードを追加（ネストSprite・配置置換との状態同期は未対応）
- [x] SpritePlayback::synchronizeでroot depthのSprite配置ID・配置フレームを照合し、移動時は再生状態を保持、置換・削除時は古い状態を破棄（呼び出し側による同期が必要）
- [x] TimelinePlayerでフレーム選択・Spriteライフサイクル同期・描画を一体化し、親タイムラインループ時に状態をリセット（ネスト個別状態・AVM連携は未実装）
- [x] ネストしたSpriteのdepth経路（例: {2,7}）別にstop/play/seek/advanceを管理して描画へ伝播（子の置換・削除に伴う自動状態破棄は未実装）
- [x] TimelinePlayer描画前にnested SpriteのDepth経路・character ID・配置フレームを照合し、子の置換・削除や親の消失時に古い再生状態を破棄
- [x] 親Spriteの配置ID・配置フレームが変化または削除された時点で、該当Depth配下のネストした再生状態を全破棄（親のidentity更新前に処理）
- [x] 中間階層のSprite置換時に子孫の再生クロックを引き継がず、孫の状態も破棄する回帰ケースを追加
- [x] PlaceObject2/3のCXFORMWITHALPHA（RGBA乗算・加算）を表示リストへ保持し、オブジェクト専用の透明レイヤーに適用して合成
- [x] PlaceObject3のBlendMode 1(Normal)・3(Multiply)・4(Screen)・8(Add)を解析し、独立レイヤーでRGB合成（残るモード・フィルターは未対応）
- [x] PlaceObject3 BlendMode 5(Lighten)・6(Darken)・7(Difference)・9(Subtract)・10(Invert)のRGBAレイヤー合成を追加（Layer・Alpha・Erase・Overlay・HardLightは未対応）
- [x] PlaceObject3 BlendMode 13（Overlay）・14（HardLight）のピクセル合成を追加。背景／前景それぞれの分岐条件に対応（テスト実行は未確認）
- [x] PlaceObject3 BlendMode 13(Overlay)・14(HardLight)のソース/背景別しきい値のRGBA合成を実装（実行テスト未確認）
- [x] PlaceObject3の単一BlurFilter（整数半径0〜16px・1〜3パス）を解析し、RGBAレイヤーへの分離型ボックスBlurを実装（Glow/DropShadow・複合フィルター・小数半径は未対応）
- [x] PlaceObject3の単一GlowFilter（外側・通常合成、整数Blur半径、strength=1.0、1〜3パス）を解析し、ぼかしたアルファ輪郭に色を付けて原画へ合成（DropShadow・内側・knockout・複数フィルターは未対応）
- [x] PlaceObject3の単一DropShadowFilterを制限付きで解析し、整数ピクセルの上下左右影・Blur・色付きアルファ合成を実装（小数距離・任意角度・inner/knockout・複数フィルターは未対応）
- [x] PlaceObject3のFilterListを最大4件まで順序保持して解析し、Blur・Glow・DropShadowを指定順にレイヤーへ適用（ほかのFilter IDや高度なオプションは明示拒否）
- [x] PlaceObject3 FilterListのColorMatrixFilter（ID 6、RGBA 4×5 float行列）を解析し、20係数を指定順に適用。非有限値・過大係数は拒否（実行検証未了）
- [x] ColorMatrixFilterのアルファ加算でオブジェクト外の透明領域全体が不透明になる問題を抑制（完全透明の入力画素は透明保持。Flashの厳密なフィルター領域拡張は今後対応）
- [x] ColorMatrixFilterの20係数演算を入力アルファが存在する外接矩形に限定し、Blur→ColorMatrix連続適用の描画テストを追加（コンパイル・実行未確認）
- [x] FilterList拡張の競合で残ったOverlay/HardLightの重複case、DropShadowテストの同名変数、未使用の重複影レンダラーを整理（ビルド実行は未確認）
- [x] PlaceObject3の単一BevelFilter（ID 3）の限定解析と外側ハイライト・シャドウ描画を実装（整数cardinal offset、strength 1.0、Blur最大16px／3パス。inner/knockout/一般角度は未対応）
- [x] GradientGlowFilter（ID 4）の2〜16色停止点・RGBA/比率解析と色補間付き外側発光を限定実装（angle/distance=0、整数Blur、strength=1、inner/knockout未対応。ビルド未検証）
- [x] GradientBevelFilter（ID 7）の2〜16色停止点・RGBA/比率・整数cardinal displacement解析と、ぼかしたアルファ差分による外側グラデーション縁取りを限定実装（inner/knockout/一般角度は未対応。ビルド未検証）
- [x] GradientGlow／GradientBevelの色停止点補間を共通化し、先頭比率未満・末尾比率超過でのunsigned減算アンダーフローを回避して端点色へクランプ（ビルド・実行未検証）
- [x] BlurFilterの分離型X/Yパスで毎回フルRGBA配列を複製していた処理を、再利用可能なscratchバッファ1個とswapへ変更（画素計算は維持。ビルド・回帰テスト未実行）

- [x] C ABIからTimeline/FrameAssetsを構築し、指定フレームを呼び出し側のRGBAバッファへ描画（stride・サイズ検証、失敗時バッファ不変、例外のC境界遮断。ビルド未検証）
- [x] C ABIの独立TimelinePlayerを追加し、seek/advance/pause/resume/state/render RGBAを公開（ActionScript・自動クロックなし、ビルド未検証）
- [ ] C ABIの描画失敗時診断メッセージ、実行テスト、フレームの背景色適用、オーディオ、イベント入力を整備

- [x] PlaceObject2/3のClipDepthを表示リストへ保持し、深度範囲内のDisplayObjectをマスクのアルファで合成（入れ子マスク、深度別期限管理、通常合成の基本経路。ビルド・実SWF未検証）
- [ ] マスク対象の非Normal Blend/複雑なSprite・フィルターとの併用、アルファ境界AA精度、オフスクリーンレイヤー削減、実SWFによる互換検証

- [x] ClipDepth合成の一時RGBAサーフェスをオブジェクト間で再利用し、各描画前に透明クリア（フレーム内の繰り返し全画面確保を削減。ビルド・描画検証未実施）
- [x] ClipDepthマスク輪郭生成の一時RGBAサーフェスもフレーム内で再利用し、表示リストの深度が厳密昇順であることとClipDepthの大小を検証（ビルド・描画検証未実施）
- [ ] ClipDepthの深度・複数マスク・Sprite入れ子・フィルターとの複合描画を実SWFで検証し、不透明度と境界AAを調整

- [x] AVM1のPlay(0x06)、Stop(0x07)、GotoFrame(0x81)をtyped VMのオプトイン実行で処理し、TimelinePlayerへフレーム単位でトランザクション反映（フレーム範囲外と未対応命令は拒否、既存の自動実行なし。ビルド・実行未検証）
- [ ] GotoLabel、GotoFrame2、ActionCallFunction、Sprite別実行コンテキスト、フレーム遷移時のDoAction実行順と同一フレーム再実行制御を整備

- [x] FrameLabel（タグ43）のラベル名をShowFrameに保持し、AVM1 GoToLabel（0x8C）の名前解決とルートTimeline遷移を実装（未知・重複ラベルを拒否、変数と再生状態のトランザクション維持。ビルド・実SWF未検証）
- [ ] GotoFrame2の数値（1始まり）・ラベル指定とplay/stopフラグを限定実装（scene biasは拒否、ビルド未検証）。Sprite単位のラベル解決、DoActionのフレーム入場時実行・再実行制御、ラベル名の文字エンコーディング互換を検証

- [x] AVM1 GotoFrame2 (0x9F) の数値フレーム・文字列ラベルと再生フラグをtyped VMで解析し、ルートTimelineへトランザクション反映（scene biasや不正フラグは明示拒否、実行未検証）

- [x] root Timeline の明示的なフレーム入場API `execute_frame_entry_actions()` と `FrameActionState` を追加。同一フレームのDoAction重複実行を抑制し、Goto後の移動先は次の明示呼び出しで実行、失敗時は入場状態を更新しない（ビルド・実行未検証）
- [ ] 一度離れて同じフレームへ再入場するケース、seek/reset/ループ時の実行契約、Spriteごとの独立AVM1コンテキストと実行予算の統合を検証・実装

- [x] root depth単位のSprite AVM1インスタンス状態（character ID、配置フレーム、変数、最終入場フレーム）を追加。SpriteのDoAction、Stop/Play/GotoFrame/ラベルを明示的に実行し、変数と再生状態を一括コミット。置換・消失時の破棄、インスタンス数上限を実装（ビルド・実行未検証）
- [ ] ネストしたSpriteの深度経路別AVM1状態、ループ・seek・配置移動と再入場の厳密な同期、タイムライン内の共通命令予算、実SWF互換性を検証

- [x] ネストSpriteのAVM1実行状態を深度パス別に保持し、親・祖先のcharacter ID/配置フレームを照合して置換時に旧変数を破棄。削除時の子孫状態と再生オーバーライドの破棄、深度32・インスタンス数・バイト予算を制限（ビルド・実機検証未実施）
- [ ] ネストSpriteの独立クロックの進行、親子のGoto直後の入場順序、全VM横断の命令予算、実SWF互換性と描画との同期を検証・完成させる

- [x] ネストSprite AVM1の祖先IDを兄弟間で共有してしまう巡回バグを修正。子ごとに祖先パスを独立コピーしてキャッシュ照合し、兄弟の変数状態が誤って破棄されないことを回帰テストへ追加（ビルド未実行）
- [ ] 独立クロックの停止・再開と親ループを同時に扱う統合テストを実機／QEMU環境で実行し、表示Spriteとのフレーム一致を確認

- [x] AVM1 TimelineCommandに明示的Play/Stop指定フラグを追加し、無操作フレームでSpriteの再生オーバーライドを生成してクロック進行を停止してしまう不具合を修正。Stopは現在のローカルフレームを保持し、子Spriteにも適用（ビルド・実行未検証）
- [ ] SpriteのループやGotoによる入場時イベント、親停止時の子クロック進行規則を表示レンダラーと照合し、統合テストで保証

- [x] ネストSpriteの祖先置換後に古い再生クロックを参照してしまう不具合を修正。新しいインスタンスは配置時のローカルフレームから開始し、Gotoで移動した際は子孫スナップショットを新しいフレームから選択する（実行テスト未検証）。
- [ ] 親子の停止・再開、Goto後のDoAction再入場、ループ時の子Spriteのライフサイクルを描画結果と同期し、Mesonで実行テストする。

- [x] ルートTimeline向け `advance_root_actions()` を追加。複数tickを1フレームずつ進めてDoActionを処理し、Stopで中断、途中のAVM1エラー時にはPlayer・変数・FrameActionStateをまとめてロールバック。tick上限を設置（ビルド・実行未検証）
- [ ] ルートtickとSprite/nested Spriteの実行を統合し、親子間の命令順序・共通予算・再入場、ループとgoto時の挙動を実SWFで検証

- [x] `advance_timeline_actions()` を追加し、1tickごとにルートDoAction→ルート深度Sprite→ネストSpriteの明示実行経路を統合。再生状態・ルート変数・Sprite変数・入場状態を実行成功時のみ一括反映（ビルド・実行テスト未検証）。
- [ ] AVM1親子スクリプトのイベント順序、Spriteの独立時刻進行、共有命令予算、実際のSWF互換性を統合テストで確定する。

- [x] `advance_timeline_actions()` のルート・Sprite・ネストSprite・複数tick全体で共有するAVM1命令実行予算を追加。typed VMが実行命令ごとに共有カウンターを減算し、超過時には統合処理をエラー終了して再生状態を反映しない（未ビルド・未実行）。
- [ ] 個別AVM1 APIも含めた全体の命令・メモリ予算と、実行時間を計測する統合ベンチマークを整備。

- [x] typed AVM1に `max_value_bytes` を追加し、変数名・変数値・実行スタック値の概算合計を制限。Push/SetVariableなどの実行後に予算超過を拒否し、呼び出し側への変数確定を防止する（ビルド・実行テスト未確認）。
- [ ] AVM1の文字列・変数の正確なメモリアロケーション制御、共有メモリ予算、長大SWFのベンチマークを追加する。

- [x] 統合AVM1実行後にルート変数とroot/nested Spriteの保持変数を横断して概算メモリ量を検査。`max_value_bytes`超過時は再生位置・変数・Sprite状態を一括ロールバック（ヒープ確保量の厳密な上限ではない。ビルド・実行未確認）。
- [ ] 一時スタック・変数コピー・文字列アロケーションを含めたピークメモリ上限をallocatorレベルで実装し、統合回帰テストと実SWFで検証。

- [x] AVM1統合実行で状態をコピーする前に概算保持値の3世代分の余裕を検査。巨大Push文字列は割り当て前に長さを拒否し、メモリ超過時の状態ロールバックを維持（アロケータの厳密なピーク保証ではない・未ビルド）。
- [ ] std::pmrまたは専用アロケータによるヒープ確保上限、計測付き負荷試験、コピー削減と実測ベンチマークを整備。
