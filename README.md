# 透過ロゴ表示区間検出 logoframe for AviSynth+ and FFmpeg

## 概要

AviSynth+はVer3.5.0からNative Linuxをサポートした。  
これは[sogaani氏][1]がLinuxに移植された[logoframe][2]をAviSynth+3.5.xで動作するように改造したものである。  

また、従来のAVS入力に加え、FFmpegが対応する動画ファイルを直接入力できる。  
動画直接入力では、[dtvindex][3]が作成する共通フレーム番号を使用する。
ロゴ検出処理は変更せず、どちらの入力方式も同じ輝度面を既存処理へ渡す。  

LGDを用意していないチャンネルでは、チャンネル名を指定して検出用LGDを自動生成できる。
作成したLGDはチャンネルごとに世代管理し、通常は最新世代を使用する。動画に対する一致率が指定値を下回った場合は、新しい世代を生成して解析を再試行する。

[1]:https://github.com/sogaani
[2]:https://github.com/sogaani/JoinLogoScp/tree/master/logoframe
[3]:https://github.com/tobitti0/dtvindex

## FFmpeg機能のAviSynth+入力との互換性

FFmpeg直接入力とdtvindex入力は、ロゴ検出ロジックを変更せず、入力部分だけを拡張している。

MPEG-2放送TS 3本の全フレームをAviSynth+／L-SMASH Works入力と比較した場合、ロゴ表示区間の出力は3本すべてでバイト単位に一致した。  
ChapterEXEおよびJoinLogoScpまで組み合わせた最終Trimも、放送TS 5本すべてでAviSynth+入力と同一になった。  

この数値はMPEG-2放送TSに対する互換性の目安であり、異なるコーデック、破損状態、タイムスタンプ構成で同じ結果を保証するものではない。  

## 使用方法

`src`で`make`を実行すると、利用可能な入力機能を自動検出してビルドする。

```console
make
```

AviSynth入力は`avisynth.h`が見つかった場合に有効になる。dtvindex入力は
次の順序で検出する。

1. `DTVINDEX_DIR`で指定したソースツリー
2. `src/libdtvindex.a`と`src/include/dtvindex/dtvindex.hpp`
3. `pkg-config`で検出できるインストール済みdtvindex
4. `logoframe`と同じ親ディレクトリにあるdtvindexソースツリー

dtvindex入力に必要なFFmpeg開発ライブラリは次のとおり。

```
libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

AviSynthヘッダーが標準以外の場所にある場合は`AVISYNTH_CFLAGS`を
指定できる。入力機能は明示的に有効化または無効化できる。

```console
make WITH_AVISYNTH=yes WITH_DTVINDEX=no
make WITH_AVISYNTH=no WITH_DTVINDEX=yes
make AVISYNTH_CFLAGS=-I/path/to/avisynth
make DTVINDEX_DIR=/path/to/dtvindex
```

起動時には、ビルドで有効になった入力機能を表示する。

```text
logoframe 1.21: AviSynth=enabled, dtvindex=enabled
```

実行方法は次のとおり。  

```console
logoframe 動画またはAVSファイル名 -logo ロゴファイル名 -oa 出力ファイル名 他オプション
```

`.avs`は従来どおりAviSynth+で、それ以外の入力はdtvindexを経由してFFmpegで読み込む。  
FFmpegでの動画ファイルの初回読み込み時には同じ場所へ`.dtvi`を作成し、2回目以降は  
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

### 自動生成LGDの精度と処理時間の目安

MPEG-2放送TS 5本について、自動生成したLGDと既存LGDの検出結果を比較した。全40境界のうち、出力行全体では37境界が一致し、主要な境界フレームは38境界が一致した。残る2境界も既存LGDとの差は1フレームだった。

同じDocker環境で既存のインデックスを再利用した場合、自動生成を含む処理時間は既存LGDを指定した場合と比べて平均約6.3秒（約14.5%）増加した。これらは使用した映像に対する参考値であり、放送局ロゴの表示時間、背景の変化量、映像形式などによって結果は変わる。

その他logoframe自体の詳細はオリジナルの[readme][4]を参照してください。

[4]:https://github.com/tobitti0/logoframe/blob/master/readme.txt

## 著作表示

- オリジナルのlogoframeおよびロゴ検出処理: [Yobi氏][5]
- Linux／AviSynth対応: [sogaani氏][1]
- LGD生成の背景選別、画素回帰およびLGD形式: [MakKi氏のロゴ解析プラグイン][6]
- AviSynth+対応、FFmpeg直接入力、dtvindex連携などの拡張: tobitti0

[5]:https://github.com/yobibi
[6]:https://github.com/makiuchi-d/delogo-aviutl