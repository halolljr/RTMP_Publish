#pragma once
#include <functional>
#include <thread>
#include <atomic>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoCapture {
public:
	VideoCapture();
	~VideoCapture();

	bool open(std::string device_name);
	void close();
	AVFrame* captureFrame(); // 返回转换成 YUV420P 的帧

	int getWidth() {
		return fmt_ctx->streams[stream_index]->codecpar->width;
	}
	int getHeight() {
		return fmt_ctx->streams[stream_index]->codecpar->height;
	}
	int getFps() {
		return av_q2d(fmt_ctx->streams[stream_index]->avg_frame_rate);
	}
private:
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* dec_ctx = nullptr;
	SwsContext* sws_ctx = nullptr;
	int stream_index = -1;
	AVFrame* yuv_frame = nullptr;
};