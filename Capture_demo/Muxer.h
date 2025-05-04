#pragma once
#include <iostream>
#include <mutex>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}
class Muxer
{
public:
	Muxer();
	~Muxer();
	bool Open(const char* file_name,AVCodecContext* video_ctx,AVCodecContext* audio_ctx);
	void Close();
	int sendPacket(AVPacket* pkt);
	AVFormatContext* Get_AVFormatContext() {
		return ofmt_ctx;
	}
	AVStream* Get_Video_AVStream() {
		return video_st;
	}
	AVStream* Get_Audio_AVStream() {
		return audio_st;
	}
private:
	std::mutex mtx;
	std::string out_put_filename;
	//封装容器输出
	AVFormatContext* ofmt_ctx = nullptr;
	//封装流
	AVStream* video_st = nullptr;
	AVStream* audio_st = nullptr;
};

