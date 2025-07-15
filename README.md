# ncmtomp3

采用c语言编写

本人只写了ui

ncm转化的库 [taurusxin/ncmdump: 转换网易云音乐 ncm 到 mp3 / flac. Convert Netease Cloud Music ncm files to mp3/flac files.](https://github.com/taurusxin/ncmdump?tab=readme-ov-file)

学习用途 ， 如有侵权联系本人删除



自行编译（记得安装相关支持）

```windres app.rc -O coff -o app.res```

```gcc -o ncm ncm.c app.res pkg-config --cflags --libs gtk+-3.0 -mwindows -lshell32 -static-libstdc++ -static-libgcc```