#pragma once
#include <functional>
#include <thread>
#include <atomic>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

class VideoCapture {
public:
	bool Open(const std::string& device = "video=Integrated Camera");
	void SetCallback(std::function<void(uint8_t* data[], int linesize[], int width, int height, int64_t pts)> cb);
	void Start();
	void Stop();

	int GetWidth() const;
	int GetHeight() const;
	int GetFPS() const;
	AVPixelFormat GetPixelFormat() const;

private:
	void CaptureLoop();

	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* codec_ctx = nullptr;
	AVFrame* frame = nullptr;
	AVPacket* pkt = nullptr;
	int video_stream_index = -1;
	std::thread thread;
	std::atomic<bool> running{ false };

	std::function<void(uint8_t* data[], int linesize[], int width, int height, int64_t pts)> callback;
};
