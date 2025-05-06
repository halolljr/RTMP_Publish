#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
}
class AudioEncoder {
public:
	AudioEncoder();
	~AudioEncoder();

	bool open(int sample_rate, int channels, int bitrate);
	void close();
	bool encode(AVFrame* frame, AVPacket* pkt);
	AVCodecContext* GetCodecContext() {
		return enc_ctx;
	}
private:
	AVCodecContext* enc_ctx = nullptr;
	int64_t pts = 0;
};
