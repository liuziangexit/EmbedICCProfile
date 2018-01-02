# EmbedICCProfile
向 JPEG 图像中嵌入 ICC 色彩配置文件。

<h2>什么是 ICC 色彩配置文件</h2>
<p>
  ICC 色彩配置文件为应用程序提供参考，指示图像色彩的正确显示方式。<br>
  <img src="/web/without-icc.JPG"><img src="/web/with-icc.JPG"><br>
  上面是两张由 iPhone 7 拍摄的同一张照片的局部对比图，第一张图片没有嵌入 ICC 配置文件，而第二张图片使用本程序嵌入了正确的配置文件有。<br>
  由于 iPhone 7 拍摄的照片使用 Display P3 色域存储，所以当这张图片出现在其它 sRGB 设备上时，如果没有正确的色彩配置文件的指示，应用程序将无法显示准确的色彩。
</p>
<h2>此程序如何使用</h2>
<p>
  推荐的做法是自己编译源代码，但是我也提供了编译后的二进制文件。您可以在 bin 文件夹找到此程序的二进制文件(使用 Visual Studio 2015 编译)。<br>
  用法是 EmbedICCProfile JPG文件 ICC文件<br>
  如: EmbedICCProfile 1.jpg "Display P3.icc"<br>
  您可能需要键入文件的绝对路径，此外，当文件名包含空格时，您应该使用英文引号将其括起。<br>
  运行后，将会将指定的 ICC 配置文件(参数1)嵌入到指定的 JPG 文件(参数2)。
</p>
<h2>跨平台</h2>
<p>
  此代码仅在 Windows 10 / Visual Studio 2015 环境下编译通过。<br>
  读取文件那部分代码(liuzianglib中)目测在 g++ 或 clang 下是过不了编译的，不过你只需要自己写一个读取文件的代码就好了。<br>
  此程序核心代码 EmbedICCProfile 函数理论上是可以跨平台的，没有什么问题(不过我没试过)。
</p>
