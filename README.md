# 透過ロゴ表示区間検出 logoframe for AviSynth+ and FFmpeg
## 概要
AviSynth+はVer3.5.0からNative Linuxをサポートした。  
これは[sogaani氏][1]がLinuxに移植された[logoframe][2]をAviSynth+3.5.xで動作するように改造したものである。  

また、従来のAVS入力に加え、FFmpegが対応する動画ファイルを直接入力できる。  
`future/dtvindex`ブランチの動画直接入力では、[dtvindex][3]が作成する共通フレーム番号を使用する。  
ロゴ検出処理は変更せず、どちらの入力方式も同じ輝度面を既存処理へ渡す。  

[1]:https://github.com/sogaani
[2]:https://github.com/sogaani/JoinLogoScp/tree/master/logoframe
[3]:https://github.com/tobitti0/dtvindex

## FFmpeg機能のAviSynth+入力との互換性

FFmpeg直接入力とdtvindex入力は、ロゴ検出ロジックを変更せず、入力部分だけを拡張している。

MPEG-2放送TS 3本の全フレームをAviSynth+／L-SMASH Works入力と比較した場合、ロゴ表示区間の出力は3本すべてでバイト単位に一致した。  
ChapterEXEおよびJoinLogoScpまで組み合わせた最終Trimも、放送TS 5本すべてでAviSynth+入力と同一になった。  

この数値はMPEG-2放送TSに対する互換性の目安であり、異なるコーデック、破損状態、タイムスタンプ構成で同じ結果を保証するものではない。  

## 使用方法
`logoframe`と`dtvindex`を同じ親ディレクトリへ配置し、AviSynth+とFFmpegの  
開発用ライブラリを導入の上、srcフォルダで`make`を実行するとビルドできる。  
FFmpeg開発ライブラリで必要なものは次のとおり。  
```
libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```
実行方法は次のとおり。  
````
logoframe 動画またはAVSファイル名 -logo ロゴファイル名 -oa 出力ファイル名 他オプション
````

`.avs`は従来どおりAviSynth+で、それ以外の入力はdtvindexを経由してFFmpegで読み込む。  
FFmpegでの動画ファイルの初回読み込み時には同じ場所へ`.dtvi`を作成し、2回目以降は  
元ファイルとの整合性を検証して再利用する。  

````
logoframe input.ts -logo logo.lgd -oa output.txt
logoframe input.avs -logo logo.lgd -oa output.txt
````
その他logoframe自体の詳細はオリジナルの[readme][4]を参照してください。

[4]:https://github.com/tobitti0/logoframe/blob/master/readme.txt

## 著作表示

- オリジナルのlogoframeおよびロゴ検出処理: [Yobi氏][5]
- Linux／AvxSynth対応: [sogaani氏][1]
- AviSynth+対応、FFmpeg直接入力、dtvindex連携などの拡張: tobitti0

[5]:https://github.com/yobibi
