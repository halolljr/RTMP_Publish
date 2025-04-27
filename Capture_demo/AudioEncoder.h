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
	AVPacket* encode(AVFrame* frame); // 编码一帧，返回编码后的Packet（需要释放）

private:
	AVCodecContext* enc_ctx = nullptr;
	int64_t pts = 0;
};
