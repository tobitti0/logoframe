# 透過ロゴ表示区間検出 logoframe for AviSynth+ and dtvindex

## 概要

AviSynth+はVer3.5.0からNative Linuxをサポートした。  
これは[sogaani氏][1]がLinuxに移植された[logoframe][2]をAviSynth+3.5.xで動作するように改造し、Windows版のAviSynth入力にも対応したものである。

また、従来のAVS入力に加え、FFmpegが対応する動画ファイルを
[dtvindex][3]経由で入力できる。dtvindexがFFmpegで映像をデコードし、
作成した共通フレーム番号を使用する。ロゴ検出処理は変更せず、
AviSynth入力とdtvindex入力のどちらも同じ輝度面を既存処理へ渡す。

LGDを用意していないチャンネルでは、チャンネル名を指定して検出用LGDを自動生成できる。
作成したLGDはチャンネルごとに世代管理し、通常は最新世代を使用する。動画に対する一致率が指定値を下回った場合は、新しい世代を生成して解析を再試行する。

[1]:https://github.com/sogaani
[2]:https://github.com/sogaani/JoinLogoScp/tree/master/logoframe
[3]:https://github.com/tobitti0/dtvindex

## 使用方法

`src`で`make`を実行すると、利用可能な入力機能を自動検出してビルドする。

```console
make
```

AviSynth入力に必要なC APIヘッダーと動的ロード処理はソースツリーに同梱している。
ビルド時にAviSynthの開発用ヘッダーやライブラリは不要で、実行時にWindowsでは
`avisynth.dll`、LinuxではAviSynth+の共有ライブラリを読み込む。
dtvindex経由の動画ファイル入力は次の順序で検出する。

1. `DTVINDEX_DIR`で指定したソースツリー
2. `src/libdtvindex.a`と`src/include/dtvindex/dtvindex.hpp`
3. `pkg-config`で検出できるインストール済みdtvindex
4. `logoframe`と同じ親ディレクトリにあるdtvindexソースツリー

dtvindex経由の動画ファイル入力に必要なFFmpeg開発ライブラリは次のとおり。

```
libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

入力機能は明示的に有効化または無効化できる。

```console
make WITH_AVISYNTH=yes WITH_DTVINDEX=no
make WITH_AVISYNTH=no WITH_DTVINDEX=yes
make DTVINDEX_DIR=/path/to/dtvindex
```

### Windows版のビルド

64 bit版MinGW-w64、`mingw32-make`、Windows向けに静的ビルドした
FFmpegの開発ファイルを用意し、その`pkg-config`定義を検索できる状態で
`src`ディレクトリから次を実行する。

```bat
compile.cmd
```

このコマンドは64 bit Windows向けの`logoframe.exe`を作成する。MinGWの
ランタイム、FFmpeg、dtvindexは静的リンクされるため、実行時に別途必要に
なるのはWindowsのシステムDLLと、AVS入力時のAviSynth+だけである。
AviSynth+は64 bit版をインストールするか、`AviSynth.dll`を
`logoframe.exe`と同じディレクトリへ配置する。

起動時には、ビルドで有効になった入力機能を表示する。

```text
logoframe 1.21
  Input : AviSynth=enabled, dtvindex=enabled
```

ヘッダー、入力情報、進捗、警告、エラーは標準エラー出力へ表示する。
通常実行時は標準出力を使用せず、解析結果は`-oa`、`-oa2`、`-o`などで
指定したファイルへ出力する。Linux版とWindows版でこの動作は共通である。

実行方法は次のとおり。  

```console
logoframe 動画またはAVSファイル名 -logo ロゴファイル名 -oa 出力ファイル名 他オプション
```

`.avs`は従来どおりAviSynth+で読み込み、それ以外はdtvindex経由の
動画ファイル入力となる。dtvindexはFFmpegで映像をデコードする。
動画ファイルの初回読み込み時には同じ場所へ`.dtvi`を作成し、2回目以降は
元ファイルとの整合性を検証して再利用する。

```console
logoframe input.ts -logo logo.lgd -oa output.txt
logoframe input.avs -logo logo.lgd -oa output.txt
```

### チャンネル別LGDの自動生成

`-channel`を指定すると、`-logo`の代わりにチャンネル別のLGDを使用する。チャンネル名は放送局名、サービスIDなど、利用環境で一意になる文字列を指定する。

```console
logoframe input.ts -channel BS11 -logo-dir ./logo -oa output.txt
logoframe input.ts -channel 211 -logo-dir ./logo -oa output.txt
```

初回は右上の領域から永続的に現れる輪郭を探し、ロゴを含む矩形を決定する。矩形外周の背景がほぼ単色になっているフレームから仮LGDを作り、仮ロゴとの一致度が高いフレームだけでLGDを再構築する。

生成対象には、ロゴが十分な強さで表示され、ロゴ背後の映像も変化する範囲が必要になる。短い同一シーンだけを`-frames`などで指定すると背景サンプルが偏るため、通常は番組全体を解析対象にする。

生成物は次のように保存する。

```text
logo/
  BS11-v0001.lgd
  BS11-v0002.lgd
  BS11.latest
```

`BS11.latest`が通常使用する世代を示す。一致率が`-logo-match`を下回ると次の世代を生成し、新しいLGDで同じ動画を再解析する。新世代は従来世代より一致率が改善した場合だけ最新版に昇格し、改善しなかった場合は従来世代へ戻して結果を出力する。一致率は、既存のLogoframe検出処理がロゴ表示と判定したフレーム数を解析フレーム数で割った値である。

主なオプションは次のとおり。

| オプション | 内容 | 初期値 |
|---|---|---:|
| `-channel name` | LGDを管理するチャンネル名 | なし |
| `-logo-dir directory` | チャンネル別LGDの保存先 | `logo` |
| `-logo-area x,y,w,h` | 自動位置探索を使わずロゴ矩形を指定 | 自動 |
| `-logo-samples number` | LGD生成で動画全体から取得するサンプル数 | `600` |
| `-logo-bg-threshold value` | 単色背景と判断する輝度差 | `12` |
| `-logo-edge-threshold value` | 永続輪郭を調べる輝度差 | `40` |
| `-logo-match percent` | 新世代を生成する一致率の下限 | `10` |

`-logo-match 0`を指定すると、一致率による自動再生成を無効にできる。自動位置探索が難しい素材では、既知のロゴ位置を`-logo-area`で指定できる。

`-logo`を指定した従来の実行方法は変更していない。`-channel`と`-logo`は同時に指定できない。

自動生成するLGDはLogoframeの区間検出に必要な輝度情報を収録する。色差情報は収録しないため、透過ロゴ除去用の完全なLGDとしては使用できない。

その他logoframe自体の詳細はオリジナルの[readme][4]を参照してください。

[4]:https://github.com/tobitti0/logoframe/blob/master/readme.txt

## 著作表示

- オリジナルのlogoframeおよびロゴ検出処理: [Yobi氏][5]
- Linux／AviSynth対応: [sogaani氏][1]
- LGD生成の背景選別、画素回帰およびLGD形式: [MakKi氏のロゴ解析プラグイン][6]
- AviSynth+対応、dtvindex経由の動画ファイル入力などの拡張: tobitti0

[5]:https://github.com/yobibi
[6]:https://github.com/makiuchi-d/delogo-aviutl
