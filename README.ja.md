# Clay LovyanGFX Renderer

[English](README.md)

[Clay](https://github.com/nicbarker/clay) のレイアウト結果を [LovyanGFX](https://github.com/lovyan03/LovyanGFX) で描画するレンダラーです。M5Stack をはじめとする ESP32 ボードの液晶に、Clay で組んだ UI を表示できます。

- Clay のすべての描画コマンドに対応しています（矩形、枠線、テキスト、画像、シザー、オーバーレイカラー、カスタム要素）
- 角丸は四隅それぞれの半径に、枠線は上下左右それぞれの太さに対応しています
- テキストは LovyanGFX のフォント（GFXfont、U8g2、BDF、VLW など）で描画します。内蔵の日本語フォントもそのまま使え、`fontSize` に応じて整数倍に拡大します
- タッチパネルとハードウェアボタンのどちらでも、両方でも、操作できます
- 画面の一部だけを持つ小さなスプライトに帯状に描いて転送するので、PSRAM のないボードでもちらつきません
- 前フレームと描画内容が同じなら転送を省きます

下記画像は `example/demo` のスクリーンショットです。

![example/demo](doc/image/demo.png)

## 快速入門

このライブラリを自分のプロジェクトで使い始めるための手順です。必要なものを確かめ、依存ライブラリを入れ、最初のプログラムを動かすところまでを説明します。

### 必要なもの

- **ボード**: LovyanGFX が対応する液晶付きの ESP32 ボード。M5Stack Basic、Core2、CoreS3 で動作を確認しています。
- **ESP32 の Arduino コア 3.x**: Clay が C++20 を必要とするためです。コア 2.x（gcc 8）では C++20 にできず、ビルドできません。
- **依存ライブラリ**: [Clay](https://github.com/nicbarker/clay)（単一ヘッダー `clay.h`）と [LovyanGFX](https://github.com/lovyan03/LovyanGFX) 1.2 以降。

### 依存ライブラリを入れる

使っているビルド環境に合わせて、次のどれか一つの方法で入れます。どの方法でも、プログラムは同じものを使えます。

#### Nix で

Nix Flakes を使っているなら、このリポジトリーを flake の入力に加えます。このリポジトリーが動作を確かめたバージョンの Clay と LovyanGFX も、その入力から取り出せます。NixOS では、ESP32 のツールチェーンが動くように FHS 環境で包んだ `arduino-cli` も使えます。

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    clay-lgfx = {
      url = "github:kakkun61/clay-lgfx-renderer";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    { nixpkgs, clay-lgfx, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = [ clay-lgfx.packages.${system}.arduino-cli ];
        # このライブラリと Clay と LovyanGFX（動作を確認したバージョン）
        shellHook = ''
          export LIBRARY_OPTS="--library ${clay-lgfx} --library ${clay-lgfx.inputs.lovyangfx} --library ${clay-lgfx.inputs.clay}"
        '';
      };
    };
}
```

`nix develop` で入ったシェルで、次のようにビルドします。

```console
arduino-cli core update-index && arduino-cli core install esp32:esp32   # 初回だけ
arduino-cli compile --fqbn esp32:esp32:m5stack_core $LIBRARY_OPTS my_sketch
arduino-cli upload --fqbn esp32:esp32:m5stack_core --port /dev/ttyUSB0 my_sketch
```

#### Arduino CLI で

LovyanGFX はライブラリマネージャーからインストールします。Clay とこのライブラリは git で取得し、`--library` で場所を渡します。Arduino IDE を使う場合は、二つのディレクトリーをスケッチブックの `libraries` フォルダーに置いても構いません。

```console
arduino-cli core update-index && arduino-cli core install esp32:esp32   # 初回だけ
arduino-cli lib install LovyanGFX
git clone https://github.com/nicbarker/clay.git
git clone https://github.com/kakkun61/clay-lgfx-renderer.git

arduino-cli compile --fqbn esp32:esp32:m5stack_core --library clay-lgfx-renderer --library clay my_sketch
```

ボードの FQBN は、Core2 なら `esp32:esp32:m5stack_core2`、CoreS3 なら `esp32:esp32:m5stack_cores3` です。

#### PlatformIO で

LovyanGFX は、このライブラリのマニフェスト（`library.json`）に従ってレジストリから自動で取得されます。Clay は PlatformIO のレジストリになく、Git のリポジトリーを依存先に指定すると別のライブラリ向けのレンダラーまでコンパイルされて失敗するので、単一ヘッダーの `clay.h` をプロジェクトの `include/` に置いてください。

```console
curl -L -o include/clay.h https://raw.githubusercontent.com/nicbarker/clay/main/clay.h
```

PlatformIO 既定の `espressif32` プラットフォームは Arduino コア 2.x なので、Arduino コア 3.x を使う [pioarduino](https://github.com/pioarduino/platform-espressif32) のプラットフォームを指定します。M5Stack Basic なら `platformio.ini` は次のとおりです。

```ini
[env:m5stack-core]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = m5stack-core-esp32
; Arduino コア 3.x ではこのボードの variant の名前が変わった
board_build.variant = m5stack_core
framework = arduino
; このライブラリ（レジストリには未登録なので Git の URL で指定する）
lib_deps = https://github.com/kakkun61/clay-lgfx-renderer.git
; include/ の clay.h をライブラリからも見えるようにする
build_flags = -isystem include
```

### 最初のプログラム

ボタンを押すと数が一つ増えるだけのプログラムです。タッチパネルのあるボードではボタンをタップし、M5Stack Basic ではボタン A と C でフォーカスを合わせてボタン B で選択します。同じものが `example/minimal/minimal.ino` にあり、`make esp32 SKETCH=example/minimal` でビルドできます。Arduino CLI なら `my_sketch/my_sketch.ino`、PlatformIO なら `src/main.ino` として保存してください。

```cpp
// The smallest useful program: a counter and a button that increments it.
//
// It runs on any ESP32 board LovyanGFX detects. The button can be tapped on a
// touch panel, or, on an original M5Stack, selected with buttons A and C and
// pressed with button B.

#define LGFX_AUTODETECT
#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

// Clay is a single header library: exactly one source file compiles its
// implementation, and this sketch is that file.
#define CLAY_IMPLEMENTATION
#include <clay.h>

#include <ClayLGFX.hpp>

// Arduino calls setup() and loop() without arguments, so the objects they
// share have to live at file scope.
static LGFX display;
static clay_lgfx::FontTable fontTable;
static clay_lgfx::Renderer renderer(fontTable);
static clay_lgfx::BandedPresenter presenter(display, renderer);
static clay_lgfx::InputController input;
static clay_lgfx::GpioButtons buttons({ 39, 38, 37 });  // Buttons A, B and C of an original M5Stack.
static uint32_t lastMillis = 0;
static int count = 0;
static char countText[24];

static void printClayError(Clay_ErrorData error) {
  Serial.printf("clay: %.*s\n", static_cast<int>(error.errorText.length), error.errorText.chars);
}

void setup() {
  Serial.begin(115200);
  display.init();

  // Any LovyanGFX font works. Font 0 is also used by Clay's debug view.
  fontTable.set(0, &lgfx::fonts::DejaVu18);

  // Clay's defaults are sized for desktops; a small screen needs far fewer elements.
  Clay_SetMaxElementCount(64);
  Clay_SetMaxMeasureTextCacheWordCount(128);
  const uint32_t size = Clay_MinMemorySize();
  const Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(size, malloc(size));
  Clay_Initialize(arena,
                  Clay_Dimensions{ static_cast<float>(display.width()), static_cast<float>(display.height()) },
                  Clay_ErrorHandler{ printClayError, nullptr });
  fontTable.attach();

  // Draw through a buffer of 40 rows, so the screen never flickers.
  presenter.begin(40);
  presenter.setClearColor(Clay_Color{ 20, 24, 32, 255 });

  // Use whatever input the board has: a touch panel, buttons, or both.
  if (display.touch()) input.setTouchDisplay(&display);
  if (display.getBoard() == lgfx::board_t::board_M5Stack) {
    buttons.begin();
    input.setButtons(&clay_lgfx::GpioButtons::reader, &buttons);
  }
  lastMillis = millis();
}

void loop() {
  const uint32_t now = millis();
  const float deltaTime = static_cast<float>(now - lastMillis) / 1000.0f;
  lastMillis = now;

  input.update(deltaTime);  // Always before Clay_BeginLayout().
  Clay_BeginLayout();

  snprintf(countText, sizeof(countText), "Count: %d", count);
  CLAY(CLAY_ID("Root"),
       { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                     .childGap = 16,
                     .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                     .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
    CLAY_TEXT((Clay_String{ false, static_cast<int32_t>(strlen(countText)), countText }),
              CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));

    // focusable() lets the buttons reach this element; it returns whether to show the focus.
    const bool focused = input.focusable(CLAY_ID("Button"));
    CLAY(CLAY_ID("Button"),
         { .layout = { .padding = { 24, 24, 10, 10 } },
           .backgroundColor = Clay_Hovered() ? Clay_Color{ 86, 156, 214, 255 } : Clay_Color{ 56, 62, 80, 255 },
           .cornerRadius = CLAY_CORNER_RADIUS(8),
           .border = { .color = { 250, 200, 60, 255 },
                       .width = CLAY_BORDER_OUTSIDE(static_cast<uint16_t>(focused ? 2 : 0)) } }) {
      // A click is a release over the element.
      if (Clay_Hovered() && Clay_GetPointerState().state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) ++count;
      CLAY_TEXT(CLAY_STRING("Press me"), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));
    }
  }

  presenter.present(Clay_EndLayout(deltaTime));
  delay(5);
}
```

プログラムの流れは次のとおりです。

1. `setup()` で、フォントの登録、Clay の初期化、描画と入力の準備をします。Clay の要素数の上限は既定ではデスクトップ向けで大きすぎるので、小さくしてからメモリーを確保します。
2. `loop()` では、毎フレーム `input.update()` で入力を Clay に渡し、`Clay_BeginLayout()` から `Clay_EndLayout()` までの間で `CLAY(...)` を使って画面を組み立て、`presenter.present()` で描きます。
3. ボタンの操作に反応するには、要素の中で `Clay_Hovered()` と `Clay_GetPointerState()` を調べます。ハードウェアボタンでも操作できるようにするには、`input.focusable()` で要素をフォーカスの対象にします。

### 次に読むもの

- [使い方](#使い方): 各部品の役割
- [フォント](#フォント): 日本語フォントやプロポーショナルフォントの使い方
- [入力](#入力): タッチパネルとボタンの扱い
- `example/demo/`: リスト、スクロール、画像、カスタム要素などを使った、もう少し大きなデモ
- [リファレンス](https://kakkun61.github.io/clay-lgfx-renderer/): API の詳細

## 使い方

このライブラリは次の部品からできています。[最初のプログラム](#最初のプログラム)では、これらを全部使っています。

- **`FontTable`**: Clay の `fontId` と LovyanGFX のフォントを対応させます。`attach()` を呼ぶと、Clay がこの表を使ってテキストの幅を測るようになります。
- **`Renderer`**: Clay の描画コマンドを、LovyanGFX のディスプレイやスプライトに描きます。
- **`BandedPresenter`**: 画面を帯状に分けて小さなスプライトに描き、ディスプレイに転送します。ちらつかず、前のフレームと同じ内容なら転送を省きます。
- **`InputController`**: タッチパネルとボタンの入力を Clay のポインターに変換します。

1 フレームの処理は、`input.update()`、`Clay_BeginLayout()`、`CLAY(...)` による画面の組み立て、`Clay_EndLayout()`、`presenter.present()` の順です。`input.update()` は必ず `Clay_BeginLayout()` より前に呼びます。

ちらつきを気にしない場合や、自前のスプライトに描きたい場合は、`renderer.render(target, commands)` で任意の `lgfx::LovyanGFX`（ディスプレイでもスプライトでも）に直接描けます。

### フォント

テキストは LovyanGFX のフォントで描きます。`lgfx::IFont` を継承したものなら何でも `FontTable` に登録でき、フォント形式の解読と文字の描画は LovyanGFX に任せます。このライブラリはフォントのデータを含みません。

- **GFXfont**: `fonts::FreeSans9pt7b` や `fonts::DejaVu12` などの内蔵フォントのほか、Adafruit GFX 形式のフォント（[shapofont](https://github.com/shapoco/shapofont)、Adafruit の fontconvert などで作ったもの）。
- **U8g2**: 内蔵の日本語フォント `fonts::lgfxJapanGothic_12`、`fonts::lgfxJapanMincho_16` など。
- **BDF**（`fonts::efont` など）、実行時に読み込む **VLW** など、LovyanGFX が扱える形式すべて。

```cpp
fontTable.set(0, &fonts::DejaVu12);
fontTable.set(1, &fonts::lgfxJapanGothic_12);   // 日本語
fontTable.set(2, &fonts::FreeSans9pt7b, 0);     // フォントにない文字は描かない
```

独自のフォントが必要なら、`lgfx::IFont` を継承して `getDefaultMetric()`、`updateFontMetric()`、`drawChar()` を実装します。グリフの持ち方、文字からグリフへの対応、送り幅の決め方は自由です。

テキストの幅は、文字ごとに `updateFontMetric()` で得た送り幅を足し合わせて求めます。描画は `drawChar()` で、位置の決め方は LovyanGFX の `drawString()` と同じです。ユニットテストで、計測した幅と描いた結果が `drawString()` と画素単位で一致することを確かめています。ただし、Clay は単語ごとに測った幅を足し合わせるので、最後の文字の描画がその送り幅からはみ出す分は幅に含めません（`textWidth()` は含めます）。

フォントにない文字は、登録時に指定した文字（既定は `?`）で描きます。それもなければ何も描かず、幅も取りません。LovyanGFX のフォントは U+FFFF までしか扱えないので、それを超える文字（絵文字の多く）も同じ扱いです。

`fontSize` が 0 ならそのままの大きさで、そうでなければフォントの文字の高さの整数倍のうち `fontSize` を超えない最大の倍率で描きます（最低 1 倍）。テキストは UTF-8 として解釈します。

Clay はテキストの計測結果をキャッシュしています。動作中に `FontTable::set()` でフォントを差し替えたときは、`Clay_ResetMeasureTextCache()` を呼んでください。

### 入力

`InputController` はタッチパネルとボタンを Clay のポインターに変換します。アプリケーションはいつも通り `Clay_Hovered()` や `Clay_PointerOver()`、`Clay_GetPointerState()` で入力に反応できます。

- **タッチパネル**: 触れた位置がそのままポインターになります。ドラッグでスクロールコンテナをスクロールでき、ドラッグした後に指を離してもタップとはみなしません。
- **ボタン**: 「前へ」「決定」「次へ」の三つのボタンでフォーカスを移動し、決定ボタンでフォーカス中の要素の中心をクリックします。フォーカスを受けられる要素は、レイアウト中に `input.focusable(id)` で宣言します。戻り値はフォーカスの枠を描くべきかどうかです。フォーカスがある間はポインターがその要素の上に置かれるので、ホバー表示がそのままフォーカス表示にもなります。
- **Core2 のタッチボタン**: `setTouchButtonZone(true)` にすると、画面の下にはみ出したタッチを三つのボタンとして扱います。

ボタンは `ButtonReader` という関数ポインターで読むので、GPIO 直結のボタンなら同梱の `GpioButtons` を、M5Unified や I/O エキスパンダー経由なら自分で書いた関数を渡せます。ボードにタッチパネルがなければ `setTouchDisplay()` を、ボタンがなければ `setButtons()` を呼ばなければよいだけです。

## グローバルな状態について

このライブラリ自身はグローバル変数を持ちません。フォントテーブル、レンダラー、入力の状態はすべてオブジェクトに入っており、Clay へのコールバックにもグローバル変数ではなく `userData` で自分自身を渡しています。

ただし、次の二つは避けられません。

- **Clay の「現在のコンテキスト」**: Clay の API は、`Clay_SetCurrentContext()` で設定されたグローバルなコンテキストを暗黙に操作する設計です。`FontTable::attach()` と `InputController::update()` はその時点で現在のコンテキストに作用します。複数のコンテキストを使う場合は、呼び出す前に `Clay_SetCurrentContext()` で切り替えてください（デモの `DemoApp::frame()` がそうしています）。
- **Arduino のエントリーポイント**: Arduino の `setup()` と `loop()` は引数を取らないので、両者で共有するディスプレイとアプリケーションのオブジェクトはファイルスコープの static 変数に置くしかありません（`example/demo/demo.ino`）。そこから先はすべてコンストラクターと引数で受け渡します。自分でメインループを持てる SDL 版（`example/demo/sim_main.cpp`）では、同じオブジェクトをスタックに置いています。

## このリポジトリーの開発

ここからは、このリポジトリー自体に手を入れる人向けの説明です。ライブラリを使うだけなら、[快速入門](#快速入門)を読めば十分です。

パッケージ管理と開発環境には Nix Flakes を使います。Clay と LovyanGFX のソースも `flake.lock` で固定されています。

```console
nix develop          # ツールチェーンが揃ったシェルに入る
make test            # ユニットテスト
make all             # ESP32 向けも含めて全部ビルドする（make esp32-setup の後）
make run-sim         # SDL シミュレーターでデモを動かす
```

`nix develop` のシェルでは `CLAY_DIR` と `LOVYANGFX_DIR` が設定されます。Makefile はこれを見てソースを探します。開発シェルの外から使う場合はこの二つを自分で渡してください。

ESP32 の Arduino コアがインストール済みなら、そのヘッダーの場所も `ARDUINO_ESP32_DIR`（コア）と `ARDUINO_ESP32_SDK_DIR`（ESP-IDF の SDK）に設定されます。コアは arduino-cli が実行時にダウンロードするので、シェルに入るたびにインストール済みの最新版を探します。VS Code の C/C++ 拡張では、`.vscode/c_cpp_properties.json` の `includePath` に `${env:ARDUINO_ESP32_DIR}/cores/esp32` などと書けば、エディターの補完やエラー表示で `Arduino.h` が見つかるようになります。VS Code が開発シェルの環境変数を受け継ぐよう、direnv 拡張を使うか、`nix develop` したシェルから `code .` で起動してください。Arduino のシンボル（`Serial` など）が `.ino` で未定義と表示されるのは、このヘッダーが見えていないためです。コアをまだ入れていなければ、`make esp32-setup` で入れてから開発シェルに入り直し、IntelliSense を再読み込みしてください。

ほかにも次のように使えます。

```console
nix build            # シミュレーター、ドキュメント、テストの実行をまとめてビルドする（result/ の下）
nix build .#doc      # ドキュメントだけをビルドする（result/html）
nix flake check      # ユニットテスト、フォーマット、スペルチェックを走らせる
nix fmt              # treefmt で全ファイルを整形する（nixfmt、clang-format）
```

スペルチェックは cspell-nix で行います。辞書に単語を足すときは cspell.yaml に追加してください。

### 実機へ書き込む

実機向けのビルドには arduino-cli を使います。ESP32 の Arduino コアは実行時にダウンロードされるビルド済みのツールチェーンを使うため、NixOS でも動くように開発シェルの `arduino-cli` は FHS 環境で包んであります。

```console
make esp32-setup                                        # 初回だけ。ESP32 コアをインストールする
make esp32                                              # M5Stack Basic 向けにビルド
make esp32 SKETCH=example/minimal                       # 最小のスケッチをビルド
make esp32-all                                          # 全スケッチを対応する全ボード向けにビルド
make upload PORT=/dev/ttyUSB0                           # ビルドして書き込む
make upload FQBN=esp32:esp32:m5stack_core2 PORT=/dev/ttyACM0
make monitor PORT=/dev/ttyUSB0                          # シリアルモニター
```

M5Stack Basic（`m5stack_core`）、Core2（`m5stack_core2`）、CoreS3（`m5stack_cores3`）向けにビルドできることを確認しています。

ESP32 の Arduino コアの版は、Makefile の `ESP32_CORE_VERSION` で固定しています（今は 3.3.12）。`make esp32-setup` はこの版を入れ、CI もこの版でビルドします。版を上げるときは、この値を変えてから `make esp32-setup` をもう一度実行してください。

## ディレクトリー構成

```
src/
  ClayLGFX.hpp            まとめて取り込むためのヘッダー
  clay_lgfx/
    renderer.hpp/.cpp     描画コマンドの処理と帯状の転送
    font.hpp/.cpp         テキストの計測、代わりの文字の扱い、UTF-8 の解釈
    font_table.hpp/.cpp   fontId とフォントの対応
    input.hpp/.cpp        タッチとボタンの入力
    geometry.hpp/.cpp     角丸と枠線のラスタライズ
test/                     ユニットテスト（LovyanGFX ごとリンクし、スプライトに描いて確かめる）
example/demo/             実機とシミュレーターで動くデモ
example/minimal/          快速入門の最初のプログラム
```

リポジトリーのルートは Arduino ライブラリの形式（`library.properties`）と PlatformIO ライブラリの形式（`library.json`）を兼ねているので、arduino-cli の `--library` にも PlatformIO の `lib_deps` にもそのまま渡せます。

## 制限

- 色のアルファ値は 0（描かない）かそれ以外（不透明に描く）としてだけ扱います。多くのパネルは画素を読み戻せないため、一般にはアルファブレンドできません。
- 画像の角丸と色合い（tint）は無視します。
- グリフは 1 ピクセル 1 ビットです。アンチエイリアスのかかったフォントやカラー絵文字は扱えません。

## ライセンス

Apache License 2.0 です。

Clay は zlib ライセンス、LovyanGFX は FreeBSD ライセンスで配布されています。
