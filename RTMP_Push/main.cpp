#include <stdio.h>
#include <iostream>
#include <memory>
#include "dlog.h"
//#include "audio.h"
//#include "aacencoder.h"
//#include "audioresample.h"
//不增加这个就要写C风格的main函数
#define SDL_MAIN_HANDLED
extern "C" {
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}
#include "pushwork.h"
#include "timeutil.h"
using namespace LQF;
#define RTMP_URL "rtmp://172.20.10.7/live/livestream"
int main()
{
	init_logger("rtmp_push.log", S_INFO);
	//    rtmpPublish(0, NULL);
	//    return 0;
		//    rtmpPublish(argc, argv);
	//        {
	//        auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {
	//                cout << "~AVFrame "  << endl;
	//                if (p) av_frame_free(&p);});
	//        }
	//        testAacEncoder("buweishui_48000_2_s16le.pcm", "buweishui.aac");
	{
		PushWork pushwork; // 可以实例化多个，同时推送多路流

		Properties properties;
		// 音频test模式
		properties.SetProperty("audio_test", 1);    
		properties.SetProperty("input_pcm_name", "RememberMe.pcm");
		// 麦克风采样属性
		properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
		properties.SetProperty("mic_sample_rate", 48000);
		properties.SetProperty("mic_channels", 2);
		// 音频编码属性
		properties.SetProperty("audio_sample_rate", 48000);
		properties.SetProperty("audio_bitrate", 256 * 1024);
		properties.SetProperty("audio_channels", 2);

		//视频test模式
		properties.SetProperty("video_test", 1);
		properties.SetProperty("input_yuv_name", "RememberMe_1920_1080_23.98.yuv");
		// 桌面录制属性
		properties.SetProperty("desktop_x", 0);
		properties.SetProperty("desktop_y", 0);
		properties.SetProperty("desktop_width", 1920); //测试模式时和yuv文件的宽度一致
		properties.SetProperty("desktop_height", 1080);  //测试模式时和yuv文件的高度一致
		//    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
		properties.SetProperty("desktop_fps", 24);//测试模式时和yuv文件的帧率一致
		// 视频编码属性
		properties.SetProperty("video_bitrate", 4*1024 * 1024);  // 设置码率

		// 使用缺省的
		// rtmp推流
		properties.SetProperty("rtmp_url", RTMP_URL);
		properties.SetProperty("rtmp_debug", 1);
		if (pushwork.Init(properties) != RET_OK)
		{
			LogError("pushwork.Init failed");
			pushwork.DeInit();
			return 0;
		}
		pushwork.Loop();

	} // 测试析构
	printf("rtmp push finish");
	return 0;
}
