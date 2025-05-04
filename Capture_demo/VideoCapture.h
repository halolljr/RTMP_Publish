#pragma once
#include <functional>
#include <thread>
#include <atomic>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libavutil/pixfmt.h >
}

class VideoCapture {
public:
	VideoCapture();
	~VideoCapture();
	bool Open(std::string device_name);
	void Start();
	void Stop();
	void Restart();
	void Close();
	template<class CallBack>
	void Set_Callback(CallBack&& cb) {
		call_back = VideoCallback(std::forward<CallBack>(cb));
	}
	int getWidth() {
		return fmt_ctx->streams[stream_index_]->codecpar->width;
	}
	int getHeight() {
		return fmt_ctx->streams[stream_index_]->codecpar->height;
	}
	int getFps() {
		return av_q2d(fmt_ctx->streams[stream_index_]->avg_frame_rate);
	}
private:
	int captureFrame();
private:
	AVDictionary* options = nullptr;
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* dec_ctx = nullptr;
	AVPacket* dec_pkt = nullptr;
	AVFrame* dec_frame = nullptr;
	int stream_index_ = -1;
	std::thread* capture_thread_ = nullptr;
	std::atomic_bool running_ = false;
	using VideoCallback = std::function<int(AVStream* input_st, enum AVPixelFormat pixformat, AVFrame* pframe, int64_t lTimeStamp)>;
	VideoCallback call_back = nullptr;
	//AVFrame* yuv_frame = nullptr;
};