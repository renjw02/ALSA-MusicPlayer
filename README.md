# ALSA-MusicPlayer
a music player designed by ALSA



## 运行说明

1. 进入`new_src`目录，输入命令`bash test.bash`，目录下会生成`music`可执行文件
2. 运行`music`，可直接运行命令`./music`，这将播放列表中的第一首歌；或运行命令`./music xxx.wav`，这将运行你指定的歌曲，但请确保歌名在`playList.txt`文件中

注：可执行文件同目录下必须存在`playList.txt`文件，其中每一行是一带有后缀的歌曲名，且该歌曲文件也应存在于同目录下



## 用户须知

运行可执行文件即开始播放歌曲，歌曲播放过程中，键盘上键入：

+ d：快进10s
+ a：快退10s
+ x：倍速降低0.5
+ c：倍速提升0.5
+ l：获取歌曲列表
+ k：下一首
+ j：上一首
+ q：退出程序

注：倍速应在0.5~2之间
