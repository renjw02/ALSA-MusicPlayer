#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include "const.h"
#include "wav.h"

#define MAX_SONGS 100

// 全局变量
pthread_t input_thread;  // 键盘输入线程
char song_path[256];  // 当前播放歌曲的路径
int playback_status = 1;  // 播放状态，0表示未播放，1表示正在播放
char songList[MAX_SONGS][256];  // 歌曲列表
int songCount = 0;  // 歌曲数量
double playback_rate = 1.0;  // 播放速度
double current_rate = 1.0;
int next = 0;   // 0切换下一首，1切换上一首

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


int main(int argc, char *argv [])
{
	int ret, rate_arg, format_arg;
	pcm_format = SND_PCM_FORMAT_S16_LE;
	rate = 44100;

    
    // 初始化ALSA PCM句柄
    pcm_name = strdup("default");
	debug_msg(snd_pcm_open(&pcm_handle,pcm_name,stream,0), "打开PCM设备");

    // 读取歌曲列表
    FILE *playlist = fopen("playList.txt", "r");
    if (playlist == NULL) {
        printf("无法打开歌曲列表文件\n");
        snd_pcm_close(pcm_handle);
        return 1;
    }
    char song_name[256];
    while (fgets(song_name, sizeof(song_name), playlist) != NULL) {
        song_name[strcspn(song_name, "\r")] = '\0';
        song_name[strcspn(song_name, "\n")] = '\0';  // 移除换行符
        strcpy(songList[songCount], song_name);  // 存储歌曲名字到歌曲列表
        songCount++;
    }
    printf("total song number:%d\n", songCount);
    fclose(playlist);

    // 创建键盘输入线程
    if (pthread_create(&input_thread, NULL, keyboard_thread, NULL) != 0) {
        printf("Failed to create keyboard thread\n");
        return 1;
    }

    // 播放歌曲
    printf("song name:%s\n", argv[1]);
    strcpy(song_path, argv[1]);
    open_music_file(song_path);

    // 设置音频参数
    set_pcm();
    

    // 音量设备初始化
    alsaVolumeInit();



    // 读取并播放歌曲
    while (1) {
        ret = fread(buff, 1, buffer_size, fp);
        if (ferror(fp)){
			printf("File read error.\n");
		}
        if(ret == 0){
			printf("%ld\n",ftell(fp));
			printf("end of music file input! \n");
			exit(1);
		}
		if(ret < 0){	
			printf("read pcm from file! \n");
			exit(1);
		}

        // printf("success\n");
        pthread_mutex_lock(&mutex);

        if (playback_status == 0) {
            // 停止当前播放，切换到下一首或上一首歌曲
            fclose(fp);
            printf("count:%d\n", songCount);

            int nextSongIndex = -1;
            for (int i = 0; i < songCount; i++) {
                // printf("song_path:%s, songList[i]:%s\n", song_path, songList[i]);
                if (strcmp(song_path, songList[i]) == 0) {
                    printf("currentIndex:%d\n", i);
                    if (next == 0) {
                        nextSongIndex = i + 1;
                        if (nextSongIndex >= songCount) {
                            nextSongIndex = 0;  // 到达歌曲列表末尾，切换到第一首歌曲
                        }
                    } else if (next == 1) {
                        nextSongIndex = i - 1;
                        if (nextSongIndex < 0) {
                            nextSongIndex = songCount - 1;  // 到达歌曲列表开头，切换到最后一首歌曲
                        }
                    }
                    break;
                }
            }
            printf("nextSongIndex: %d\n", nextSongIndex);
            if (nextSongIndex >= 0) {
                printf("nextSongIndex = %d\n", nextSongIndex);
                strcpy(song_path, songList[nextSongIndex]);  // 更新当前歌曲路径
                // fp = fopen(song_path, "r");  // 打开下一首歌曲文件
                // if (fp == NULL) {
                //     printf("无法打开下一首歌曲文件\n");
                //     break;
                // }
                printf("song path:%s\n", song_path);
                open_music_file(song_path);
                printf("change to %s\n", song_path);
                playback_status = 1;  // 开始播放下一首歌曲
            }
        }
        pthread_mutex_unlock(&mutex);

        pthread_mutex_lock(&mutex);
        if (playback_rate > current_rate) {
            current_rate = playback_rate;
            printf("new rate %.2f\n", current_rate);
            rate *= 2;
            snd_pcm_drain(pcm_handle);
            set_pcm();
        }
        else if (playback_rate < current_rate)
        {
            current_rate = playback_rate;
            printf("new rate %.2f\n", current_rate);
            rate *= 0.5;
            snd_pcm_drain(pcm_handle);
            set_pcm();
        }
        pthread_mutex_unlock(&mutex);


        // 向PCM设备写入数据,
		while((ret = snd_pcm_writei(pcm_handle,buff,frames)) < 0){
			if (ret == -EPIPE){
                /* EPIPE means underrun -32  的错误就是缓存中的数据不够 */
                printf("underrun occurred -32, err_info = %s \n", snd_strerror(ret));
		        //完成硬件参数设置，使设备准备好
                snd_pcm_prepare(pcm_handle);
            } else if(ret < 0){
				printf("ret value is : %d \n", ret);
				debug_msg(-1, "write to audio interface failed");
			}
		}
        snd_pcm_wait(pcm_handle, 1000);  // 等待播放完成
        // snd_pcm_rate_timestretch(pcm_handle, SND_PCM_RATE_TST_SPEED, playback_rate);  // 设置播放速度
    }

    fprintf(stderr, "end of music file input\n");
	
	// 关闭文件
	fclose(fp);
	snd_pcm_close(pcm_handle);
	pthread_cancel(input_thread);
	pthread_join(input_thread, NULL);
	return 0;
}

// 键盘输入线程函数
void* keyboard_thread(void* arg) {
    while (1) {
        // 读取用户输入的字符
        char c = getchar();
        // 根据用户输入来调整音量
        if (c == 'w') {
			printf("increase volume...\n");
			increaseVolume(); 
        } else if (c == 's') { 
			printf("decrease volume...\n");          
			decreaseVolume();
        } else if (c == 'm') {
			printf("mute...\n");
			setMasterVolume(0);
		} else if (c == '\n') {
			continue;
		} else if (c == 'a'){
			printf("back 10s...\n");
			int temp = divnum*10;
			if(ftell(fp)>=temp){
				fseek(fp,-temp,1);
			}
			else{
				fseek(fp,44,0);
			}
			
			temp = ftell(fp);
			printf("%ds/%ds",temp/divnum,totalSecond);
		} else if (c == 'd'){
			printf("forward 10s...\n");
			int temp = divnum*10;
			fseek(fp,temp,1);
			temp = ftell(fp);
			printf("%ds/%ds",temp/divnum,totalSecond);
		} else if (c == 'l') {
			printf("get song list...\n");
			for (int i = 0; i < songCount; i++) {
                printf("%s\n", songList[i]);
            }
        } else if (c == 'k') {
            // 加锁
            pthread_mutex_lock(&mutex);
            printf("next song...\n");
            next = 0;
            playback_status = 0;
            // 解锁
            pthread_mutex_unlock(&mutex);
        } else if (c == 'j') {
            pthread_mutex_lock(&mutex);
            printf("previous song...\n");
            next = 1;
            playback_status = 0;
            pthread_mutex_unlock(&mutex);
        } else if (c == 'i') {
            printf("x0.5 rate...\n");
            pthread_mutex_lock(&mutex);
            playback_rate *= 0.5;
            printf("set rate at %.2f \n", playback_rate);
            pthread_mutex_unlock(&mutex);
        } else if (c == 'o') {
            printf("x2 rate...\n");
            pthread_mutex_lock(&mutex);
            playback_rate *= 2;
            printf("set rate at %.2f \n", playback_rate);
            pthread_mutex_unlock(&mutex);
        } else if (c == 'q') {
            printf("exit program...\n");
            snd_pcm_close(pcm_handle);  // 关闭ALSA PCM句柄
            exit(0);
        } else {
            printf("Invalid input\n");
        }
    }
    return NULL;
}

void set_pcm() {
    // 设置音频参数
    // 在堆栈上分配snd_pcm_hw_params_t结构体的空间，参数是配置pcm硬件的指针,返回0成功
	debug_msg(snd_pcm_hw_params_malloc(&hw_params), "分配snd_pcm_hw_params_t结构体");
    // 在我们将PCM数据写入声卡之前，我们必须指定访问类型，样本长度，采样率，通道数，周期数和周期大小。
	// 首先，我们使用声卡的完整配置空间之前初始化hwparams结构
	debug_msg(snd_pcm_hw_params_any(pcm_handle, hw_params), "配置空间初始化");

	// 设置交错模式（访问模式）
	// 常用的有 SND_PCM_ACCESS_RW_INTERLEAVED（交错模式） 和 SND_PCM_ACCESS_RW_NONINTERLEAVED （非交错模式） 
	// 参考：https://blog.51cto.com/yiluohuanghun/868048
	debug_msg(snd_pcm_hw_params_set_access(pcm_handle, hw_params,SND_PCM_ACCESS_RW_INTERLEAVED), "设置交错模式（访问模式）");

	debug_msg(snd_pcm_hw_params_set_format(pcm_handle,hw_params, pcm_format), "设置样本长度(位数)");
	int dir=0;
	debug_msg(snd_pcm_hw_params_set_rate_near(pcm_handle,hw_params, &rate, &dir), "设置采样率");

	debug_msg(snd_pcm_hw_params_set_channels(pcm_handle,hw_params, 2), "设置通道数");

    // 设置缓冲区 buffer_size = period_size * periods 一个缓冲区的大小可以这么算，我上面设定了周期为2，
	// 周期大小我们预先自己设定，那么要设置的缓存大小就是 周期大小 * 周期数 就是缓冲区大小了。
	buffer_size = period_size * periods;
	
	// 为buff分配buffer_size大小的内存空间
    // TODO:是否与倍速有关
	buff = (unsigned char *)malloc(buffer_size);
    frames = buffer_size >> 2;
	debug_msg(snd_pcm_hw_params_set_buffer_size_near(pcm_handle,hw_params,&frames), "设置S16_LE OR S16_BE缓冲区");

    // 设置的硬件配置参数，加载，并且会自动调用snd_pcm_prepare()将stream状态置为SND_PCM_STATE_PREPARED
	debug_msg(snd_pcm_hw_params(pcm_handle,hw_params), "设置的硬件配置参数");
}

void open_music_file(const char *path_name){
	// TODO: 打开音频文件，输出其文件头信息
	// 直接复制part1的相关代码到此处即可
	int fd;
    FILE* wfp;
    Wav wav;
    RIFF_t riff;
    FMT_t fmt;
    Data_t data;    
    
    // printf("success\n");
    printf("path: %s\n", path_name);
    // printf("len:%d\n");
    int err = 0;
    fp = fopen(path_name,"rb");
    if (fp == NULL) {
        printf("failed to open file\n");
        exit(1);
    }

	struct stat statbuff;
	if(stat(path_name, &statbuff) < 0){
		printf("file not exist\n");
	}else{
		filesize = statbuff.st_size;
	}
    
    if (err) {
        printf("The file was not opened\n");
    }
    else {
        fread(&wav,1,sizeof(wav),fp);
		printf("wavsize:%u\n",(unsigned int)sizeof(wav));
        //fclose(fp);
        char txtn[100];
        int n = 0;
        while(path_name[n] != '\0') {
            n++;
        }
        for(int i=0;i<n-4;i++){
            txtn[i] = path_name[i];
        }
        txtn[n-4] = '.';
        txtn[n-3] = 't';
        txtn[n-2] = 'x';
        txtn[n-1] = 't';
        txtn[n] = '\0';
        wfp = fopen(txtn, "w");
		printf("%s\n",txtn);
		if(wfp){
			printf("wfpasdasd\n");
		}
        riff = wav.riff;
        fmt = wav.fmt;
        data = wav.data;
		divnum = fmt.SampleRate*fmt.NumChannels*fmt.BitsPerSample/8;
		totalSecond = riff.ChunkSize/divnum;
 
        //printf("ChunkID \t%c%c%c%c\n", riff.ChunkID[0], riff.ChunkID[1], riff.ChunkID[2], riff.ChunkID[3]);
        fprintf(wfp,"wav文件头结构体大小：\t44\n");
        fprintf(wfp,"RIFF标志： \t%c%c%c%c\n", riff.ChunkID[0], riff.ChunkID[1], riff.ChunkID[2], riff.ChunkID[3]);
        //printf("ChunkSize \t%d\n", riff.ChunkSize);
        fprintf(wfp,"文件大小： \t%d\n", riff.ChunkSize);
        //printf("Format \t\t%c%c%c%c\n", riff.Format[0], riff.Format[1], riff.Format[2], riff.Format[3]);
        fprintf(wfp,"文件格式： \t%c%c%c%c\n", riff.Format[0], riff.Format[1], riff.Format[2], riff.Format[3]);
 
        //printf("\n");
 
        //printf("Subchunk1ID \t%c%c%c%c\n", fmt.Subchunk1ID[0], fmt.Subchunk1ID[1], fmt.Subchunk1ID[2], fmt.Subchunk1ID[3]);
        fprintf(wfp,"格式块标识： \t%c%c%c%c\n", fmt.Subchunk1ID[0], fmt.Subchunk1ID[1], fmt.Subchunk1ID[2], fmt.Subchunk1ID[3]);
        //printf("Subchunk1Size \t%d\n", fmt.Subchunk1Size);
        fprintf(wfp,"格式块长度： \t%d\n", fmt.Subchunk1Size);
        //printf("AudioFormat \t%d\n", fmt.AudioFormat);
        fprintf(wfp,"编码格式代码： \t%d\n", fmt.AudioFormat);
        //printf("NumChannels \t%d\n", fmt.NumChannels);
        fprintf(wfp,"声道数： \t%d\n", fmt.NumChannels);
        //printf("SampleRate \t%d\n", fmt.SampleRate);
        fprintf(wfp,"采样频率： \t%d\n", fmt.SampleRate);
        //printf("ByteRate \t%d\n", fmt.ByteRate);
        fprintf(wfp,"传输速率： \t%d\n", fmt.ByteRate);
        //printf("BlockAlign \t%d\n", fmt.BlockAlign);
        fprintf(wfp,"数据块对齐单位： \t%d\n", fmt.BlockAlign);
        //printf("BitsPerSample \t%d\n", fmt.BitsPerSample);
        fprintf(wfp,"采样位数(长度)： \t%d\n", fmt.BitsPerSample);
		fclose(wfp);
	}
}

void alsaVolumeInit(){
    snd_mixer_selem_id_t* sid = NULL;
    const char* card = "default";
    const char* selem_name = "Master";
    //1. 打开混音设备
    int res = snd_mixer_open(&handle, 0);
    if (res < 0) {
        printf("Failed to open mixer: %s\n", snd_strerror(res));
        exit(1);
    }
    //2. attach HCTL to open mixer
    res = snd_mixer_attach(handle, card);
    if (res < 0) {
    printf("Failed to attach mixer: %s\n", snd_strerror(res));
        exit(1);
    }
    //3. Register mixer simple element class.
    snd_mixer_selem_register(handle, NULL, NULL);
    if (res < 0) {
    printf("Failed to register simple mixer element: %s\n", snd_strerror(res));
        exit(1);
    }
    //4. 取得第一個 element，也就是 Master
    snd_mixer_load(handle);
    if (res < 0) {
        printf("Failed to load mixer elements: %s\n", snd_strerror(res));
        exit(1);
    }
    //5. allocate an invalid snd_mixer_selem_id_t using standard alloca
    snd_mixer_selem_id_alloca(&sid);

    //6. 设置元素ID的位置
    snd_mixer_selem_id_set_index(sid, 0);

    //7. 设置元素ID的名字
    snd_mixer_selem_id_set_name(sid, selem_name);

    //8. 查找元素
    element_handle = snd_mixer_find_selem(handle, sid);
    if (element_handle == NULL) {
        printf("Failed to find simple mixer element %s\n", selem_name);
        exit(1);
    }
    res = snd_mixer_selem_get_playback_volume_range(element_handle, &minVolume, &maxVolume);
    if (res < 0) {
        printf("Failed to get playback volume range: %s\n", snd_strerror(res));
        exit(1);
    }
}

bool debug_msg(int result, const char *str)
{
	if(result < 0){
		printf("err: %s 失败!, result = %d, err_info = %s \n", str, result, snd_strerror(result));
		exit(1);
	}
	return true;
}

long increaseVolume()
{
    long newVolume = 0;
    if (getCurrentVolume() <= 100 - _VOLUMECHANGE) // check that we don't go above the max volume
        newVolume = getCurrentVolume() + _VOLUMECHANGE;
    else
        newVolume = 100;
	printf("Volume increased to %ld\n", newVolume);
    setMasterVolume(newVolume);
    return newVolume;
}

long decreaseVolume()
{
    long newVolume = 0;
    if (getCurrentVolume() >= 0 + _VOLUMECHANGE) // check that we won't go below minimum volume
        newVolume = getCurrentVolume() - _VOLUMECHANGE;
    else
        newVolume = 0;
	printf("Volume decreased to %ld\n", newVolume);
    setMasterVolume(newVolume);
    return newVolume;
}

int setMasterVolume(long volume)
{
    long alsaVolume = volume * (maxVolume - minVolume) / 100 ;
    if(snd_mixer_selem_set_playback_volume_all(element_handle, alsaVolume) < 0){
        if(handle)
        snd_mixer_close(handle);
        return -1;
    }
	if(volume == 0){
		printf("Now the volume is muted\n");
	}
	if(volume == 100){
		printf("Now it's at maximum volume\n");
	}
    return 0;
}

long getCurrentVolume()
{
    long alsaVolume;
    if(snd_mixer_selem_get_playback_volume(element_handle, SND_MIXER_SCHN_MONO, &alsaVolume) < 0){
        if(handle)
            snd_mixer_close(handle);
            return -1;
        }
    return (alsaVolume*100)/(maxVolume - minVolume);
}