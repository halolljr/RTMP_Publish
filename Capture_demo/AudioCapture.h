#pragma once
#include <functional>
#include <thread>
#include <atomic>
#include <string>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
}

class AudioCapture {
public:
	AudioCapture();
	~AudioCapture();

	bool open(std::string device_name);
	void close();
	AVFrame* captureFrame(); // 返回重采样到目标格式的帧

private:
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* dec_ctx = nullptr;
	SwrContext* swr_ctx = nullptr;
	int stream_index = -1;
	AVFrame* pcm_frame = nullptr;
};