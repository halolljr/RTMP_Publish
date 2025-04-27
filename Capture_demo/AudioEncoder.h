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
	AVPacket* encode(AVFrame* frame); // ����һ֡�����ر�����Packet����Ҫ�ͷţ�

private:
	AVCodecContext* enc_ctx = nullptr;
	int64_t pts = 0;
};
