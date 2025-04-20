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
#define RTMP_URL "rtmp://172.20.10.7/live/livestream1"
#define RTMP_URL_2 "rtmp://172.20.10.7/live/livestream2"
int main()
{
	init_logger("rtmp_push.log", S_INFO);
	//注释部分为推送多个直播流(在最后的while循环里面会模拟推送30秒，之后结束)
	//std::thread t1 = std::thread([]()	//测试多路流
	//	{
	//		PushWork pushwork; // 可以实例化多个，同时推送多路流

	//		Properties properties;
	//		// 音频test模式
	//		properties.SetProperty("audio_test", 1);
	//		properties.SetProperty("input_pcm_name", "RememberMe.pcm");
	//		// 麦克风采样属性
	//		properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
	//		properties.SetProperty("mic_sample_rate", 48000);
	//		properties.SetProperty("mic_channels", 2);
	//		// 音频编码属性
	//		properties.SetProperty("audio_sample_rate", 48000);
	//		properties.SetProperty("audio_bitrate", 256 * 1024);
	//		properties.SetProperty("audio_channels", 2);

	//		//视频test模式
	//		properties.SetProperty("video_test", 1);
	//		properties.SetProperty("input_yuv_name", "RememberMe_1920_1080_23.98.yuv");
	//		// 桌面录制属性
	//		properties.SetProperty("desktop_x", 0);
	//		properties.SetProperty("desktop_y", 0);
	//		properties.SetProperty("desktop_width", 1920); //测试模式时和yuv文件的宽度一致
	//		properties.SetProperty("desktop_height", 1080);  //测试模式时和yuv文件的高度一致
	//		//    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
	//		properties.SetProperty("desktop_fps", 24);//测试模式时和yuv文件的帧率一致
	//		// 视频编码属性
	//		properties.SetProperty("video_bitrate", 4 * 1024 * 1024);  // 设置码率

	//		// 使用缺省的
	//		// rtmp推流
	//		properties.SetProperty("rtmp_url", RTMP_URL);
	//		properties.SetProperty("rtmp_debug", 0);
	//		if (pushwork.Init(properties) != RET_OK)
	//		{
	//			LogError("pushwork.Init failed");
	//			pushwork.DeInit();
	//			return 0;
	//		}
	//		while (true) {

	//		}
	//		// 测试析构
	//	});
	//std::thread t2 = std::thread([]()	//测试多路流
	//	{
	//		PushWork pushwork; // 可以实例化多个，同时推送多路流

	//		Properties properties;
	//		// 音频test模式
	//		properties.SetProperty("audio_test", 1);
	//		properties.SetProperty("input_pcm_name", "RememberMe.pcm");
	//		// 麦克风采样属性
	//		properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
	//		properties.SetProperty("mic_sample_rate", 48000);
	//		properties.SetProperty("mic_channels", 2);
	//		// 音频编码属性
	//		properties.SetProperty("audio_sample_rate", 48000);
	//		properties.SetProperty("audio_bitrate", 256 * 1024);
	//		properties.SetProperty("audio_channels", 2);

	//		//视频test模式
	//		properties.SetProperty("video_test", 1);
	//		properties.SetProperty("input_yuv_name", "RememberMe_1920_1080_23.98.yuv");
	//		// 桌面录制属性
	//		properties.SetProperty("desktop_x", 0);
	//		properties.SetProperty("desktop_y", 0);
	//		properties.SetProperty("desktop_width", 1920); //测试模式时和yuv文件的宽度一致
	//		properties.SetProperty("desktop_height", 1080);  //测试模式时和yuv文件的高度一致
	//		//    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
	//		properties.SetProperty("desktop_fps", 24);//测试模式时和yuv文件的帧率一致
	//		// 视频编码属性
	//		properties.SetProperty("video_bitrate", 4 * 1024 * 1024);  // 设置码率

	//		// 使用缺省的
	//		// rtmp推流
	//		properties.SetProperty("rtmp_url", RTMP_URL_2);
	//		properties.SetProperty("rtmp_debug", 0);
	//		if (pushwork.Init(properties) != RET_OK)
	//		{
	//			LogError("pushwork.Init failed");
	//			pushwork.DeInit();
	//			return 0;
	//		}
	//		while (true) {

	//		}
	//		// 测试析构
	//	});
	//while (true) {
	//  std::this_thread::sleep_for(std::chrono::seconds(30));
		//if (t1.joinable()) {
		//	t1.join();
		//}
		//if (t2.joinable()) {
		//	t2.joinable();
		//}
	//}
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
		properties.SetProperty("video_bitrate", 4 * 1024 * 1024);  // 设置码率

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
		// 测试析构
	}
	printf("rtmp push finish");
	return 0;
}
