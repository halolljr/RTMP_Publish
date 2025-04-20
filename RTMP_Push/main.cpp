#include <stdio.h>
#include <iostream>
#include <memory>
#include "dlog.h"
//#include "audio.h"
//#include "aacencoder.h"
//#include "audioresample.h"
//�����������ҪдC����main����
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
	//ע�Ͳ���Ϊ���Ͷ��ֱ����(������whileѭ�������ģ������30�룬֮�����)
	//std::thread t1 = std::thread([]()	//���Զ�·��
	//	{
	//		PushWork pushwork; // ����ʵ���������ͬʱ���Ͷ�·��

	//		Properties properties;
	//		// ��Ƶtestģʽ
	//		properties.SetProperty("audio_test", 1);
	//		properties.SetProperty("input_pcm_name", "RememberMe.pcm");
	//		// ��˷��������
	//		properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
	//		properties.SetProperty("mic_sample_rate", 48000);
	//		properties.SetProperty("mic_channels", 2);
	//		// ��Ƶ��������
	//		properties.SetProperty("audio_sample_rate", 48000);
	//		properties.SetProperty("audio_bitrate", 256 * 1024);
	//		properties.SetProperty("audio_channels", 2);

	//		//��Ƶtestģʽ
	//		properties.SetProperty("video_test", 1);
	//		properties.SetProperty("input_yuv_name", "RememberMe_1920_1080_23.98.yuv");
	//		// ����¼������
	//		properties.SetProperty("desktop_x", 0);
	//		properties.SetProperty("desktop_y", 0);
	//		properties.SetProperty("desktop_width", 1920); //����ģʽʱ��yuv�ļ��Ŀ��һ��
	//		properties.SetProperty("desktop_height", 1080);  //����ģʽʱ��yuv�ļ��ĸ߶�һ��
	//		//    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
	//		properties.SetProperty("desktop_fps", 24);//����ģʽʱ��yuv�ļ���֡��һ��
	//		// ��Ƶ��������
	//		properties.SetProperty("video_bitrate", 4 * 1024 * 1024);  // ��������

	//		// ʹ��ȱʡ��
	//		// rtmp����
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
	//		// ��������
	//	});
	//std::thread t2 = std::thread([]()	//���Զ�·��
	//	{
	//		PushWork pushwork; // ����ʵ���������ͬʱ���Ͷ�·��

	//		Properties properties;
	//		// ��Ƶtestģʽ
	//		properties.SetProperty("audio_test", 1);
	//		properties.SetProperty("input_pcm_name", "RememberMe.pcm");
	//		// ��˷��������
	//		properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
	//		properties.SetProperty("mic_sample_rate", 48000);
	//		properties.SetProperty("mic_channels", 2);
	//		// ��Ƶ��������
	//		properties.SetProperty("audio_sample_rate", 48000);
	//		properties.SetProperty("audio_bitrate", 256 * 1024);
	//		properties.SetProperty("audio_channels", 2);

	//		//��Ƶtestģʽ
	//		properties.SetProperty("video_test", 1);
	//		properties.SetProperty("input_yuv_name", "RememberMe_1920_1080_23.98.yuv");
	//		// ����¼������
	//		properties.SetProperty("desktop_x", 0);
	//		properties.SetProperty("desktop_y", 0);
	//		properties.SetProperty("desktop_width", 1920); //����ģʽʱ��yuv�ļ��Ŀ��һ��
	//		properties.SetProperty("desktop_height", 1080);  //����ģʽʱ��yuv�ļ��ĸ߶�һ��
	//		//    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
	//		properties.SetProperty("desktop_fps", 24);//����ģʽʱ��yuv�ļ���֡��һ��
	//		// ��Ƶ��������
	//		properties.SetProperty("video_bitrate", 4 * 1024 * 1024);  // ��������

	//		// ʹ��ȱʡ��
	//		// rtmp����
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
	//		// ��������
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
		PushWork pushwork; // ����ʵ���������ͬʱ���Ͷ�·��

		Properties properties;
		// ��Ƶtestģʽ
		properties.SetProperty("audio_test", 1);
		properties.SetProperty("input_pcm_name", "RememberMe.pcm");
		// ��˷��������
		properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
		properties.SetProperty("mic_sample_rate", 48000);
		properties.SetProperty("mic_channels", 2);
		// ��Ƶ��������
		properties.SetProperty("audio_sample_rate", 48000);
		properties.SetProperty("audio_bitrate", 256 * 1024);
		properties.SetProperty("audio_channels", 2);

		//��Ƶtestģʽ
		properties.SetProperty("video_test", 1);
		properties.SetProperty("input_yuv_name", "RememberMe_1920_1080_23.98.yuv");
		// ����¼������
		properties.SetProperty("desktop_x", 0);
		properties.SetProperty("desktop_y", 0);
		properties.SetProperty("desktop_width", 1920); //����ģʽʱ��yuv�ļ��Ŀ��һ��
		properties.SetProperty("desktop_height", 1080);  //����ģʽʱ��yuv�ļ��ĸ߶�һ��
		//    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
		properties.SetProperty("desktop_fps", 24);//����ģʽʱ��yuv�ļ���֡��һ��
		// ��Ƶ��������
		properties.SetProperty("video_bitrate", 4 * 1024 * 1024);  // ��������

		// ʹ��ȱʡ��
		// rtmp����
		properties.SetProperty("rtmp_url", RTMP_URL);
		properties.SetProperty("rtmp_debug", 1);
		if (pushwork.Init(properties) != RET_OK)
		{
			LogError("pushwork.Init failed");
			pushwork.DeInit();
			return 0;
		}
		pushwork.Loop();
		// ��������
	}
	printf("rtmp push finish");
	return 0;
}
