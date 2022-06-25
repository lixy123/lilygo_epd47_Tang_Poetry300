# lilygo_epd47_Tang_Poetry300
lilygo 4.7寸墨水屏显示唐诗300首

<b>一.功能：</b><br/>
   给学生学习唐诗用的。每12小时更换显示新诗.<br/>
   希望每天能帮助背1-2首唐诗<br/>
   本机存有300首唐诗.<br/>

<b>二.硬件</b><br/>
    lilygo 4.7寸墨水屏<br/>  
    <img src= 'https://github.com/lixy123/lilygo_epd47_Tang_Poetry300/blob/main/tang.jpg?raw=true' /> <br/>
    
<b>三.软件  </b><br/>
烧录到ESP32开发板<br/>
A.软件: arduino 1.8.19<br/>
B.用到库文件:<br/>
https://github.com/espressif/arduino-esp32 版本:1.0.6<br/>
https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 墨水屏驱动<br/>
https://github.com/ivanseidel/LinkedList  先进先出队列(处理墨水屏多行汉字用到)<br/>
C.开发板选择：TTGO-T-WATCH 参数选默认 (字库文件较大，仅用到其分区定义)<br/>
注： 较新的arduino版本才有这个开发板定义: TTGO T-Watch<br/>
参考：https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library<br/>
D.选择端口，点击烧录<br/> 
