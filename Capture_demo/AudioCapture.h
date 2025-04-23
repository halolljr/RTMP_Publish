#pragma once
#include <functional>
#include <thread>
#include <atomic>
#include <string>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

class AudioCapture {
public:
	bool Open(const std::string& device = u8"audio=¶ú»ú (AirPods4)");
	void SetCallback(std::function<void(uint8_t* data, int size, int64_t pts)> cb);
	void Start();
	void Stop();

	int GetSampleRate() const;
	int GetChannels() const;
	AVSampleFormat GetSampleFormat() const;

private:
	void CaptureLoop();

	AVFormatContext* fmt_ctx = nullptr;
	AVCodecParameters* codecpar = nullptr;
	AVPacket* pkt = nullptr;
	int audio_stream_index = 0;
	std::thread thread;
	std::atomic<bool> running{ false };

	std::function<void(uint8_t* data, int size, int64_t pts)> callback;
};
