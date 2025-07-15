# ncmdump-gui

采用c语言进行编写

本库在原功能的基础上加了部分ui

ncm转化的库 [taurusxin/ncmdump: 转换网易云音乐 ncm 到 mp3 / flac. Convert Netease Cloud Music ncm files to mp3/flac files.](https://github.com/taurusxin/ncmdump?tab=readme-ov-file)

学习用途 ， 如有侵权联系本人删除

---

## 已知BUG

1.可能由于路径或名字原因出现：显示成功转换单无文件（可以用文件夹转换解决）

2.转换行为可能会被杀毒软件拦截

3.文件夹转换显示成功后会延迟出现

---

## 自行编译（记得安装相关支持）

```windres app.rc -O coff -o app.res```

```gcc -o ncm ncm.c app.res pkg-config --cflags --libs gtk+-3.0 -mwindows -lshell32 -static-libstdc++ -static-libgcc```