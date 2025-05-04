#pragma once
#include <functional>
#include <thread>
#include <atomic>
#include <string>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
}

class AudioCapture {
public:
	AudioCapture();
	~AudioCapture();

	bool Open(std::string device_name);
	void Start();
	void Stop();
	void Restart();
	void Close();
	template<class Callback>
	void Set_Callback(Callback&& cb) {
		call_back = AudioCallback(std::forward<Callback>(cb));
	}
private:
	int captureFrame();
private:
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* dec_ctx = nullptr;
	AVPacket* dec_pkt = nullptr;
	AVFrame* dec_frame = nullptr;
	int stream_index_ = -1;
	std::thread* capture_thread_ = nullptr;
	std::atomic_bool running_ = false;
	using AudioCallback = std::function<int(AVStream* input_st, AVFrame* input_frame, int64_t lTimeStramp)>;
	AudioCallback call_back = nullptr;
};