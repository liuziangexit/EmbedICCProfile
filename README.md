# EmbedICCProfile
此程序可以向 JPEG 图像中嵌入 ICC 色彩配置文件。

<h2>什么是 ICC 色彩配置文件</h2>
<p>
ICC 色彩配置文件为应用程序提供参考，指示图像色彩的正确显示方式。<br>
<img src="/web/without-icc.JPG"><img src="/web/with-icc.JPG"><br>
上面是两张由 iPhone 7 拍摄的同一张照片的局部对比图，第一张图片没有嵌入 ICC 配置文件，而第二张图片使用本程序嵌入了正确的配置文件。<br><br>
由于 iPhone 7 拍摄的照片使用 Display P3 色域存储，所以当这张图片出现在其他 sRGB 设备上时，如果没有正确的色彩配置文件的指示，应用程序将无法显示准确的色彩。<br><br>
在本例中，在 sRGB Windows 10 设备上使用 “照片” app 打开没有色彩配置文件的 Display P3 照片，显示效果将会比正确的色彩偏淡、偏紫。
</p>
<h2>安装</h2>
<p>
 
 ```
 git clone https://github.com/liuziangexit/EmbedICCProfile
 cd EmbedICCProfile/src
 cmake CMakeLists.txt
 make
 make install
 ```
 
</p>
<h2>使用</h2>
<p>
 
```
EmbedICCProfile xxx.jpg xxx.icc
```
如: EmbedICCProfile 1.jpg "Display P3.icc"<br><br>
您可能需要键入文件的绝对路径，此外，当文件名包含空格时，您应该使用英文引号将其括起。<br>
运行后，会将指定的 ICC 配置文件(参数2)嵌入到指定的 JPG 文件(参数1)。
</p>
<h2>代码库文件夹说明</h2>
<p>
example 中存放了一张使用 iPhone 7 拍摄的 Display P3 色域的照片，但是并未内嵌 ICC 色彩配置文件。您可以使用本程序向其中嵌入 Display P3 配置文件，并对比嵌入前和嵌入后的不同。<br><br>
icc-profile 中存放了许多常用色彩文件，包括 Display P3 和 sRGB 等，这是我从我的 Mac 上拷出来的。<br><br>
src 文件夹中包含了此程序的所有源代码。
</p>
