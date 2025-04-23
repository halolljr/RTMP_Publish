#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
}
class AudioEncoder {
public:
	bool Open(int in_sample_rate, int in_channels, AVSampleFormat in_sample_fmt,
		int out_sample_rate = 44100, int out_channels = 2, AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_FLTP);
	AVPacket* Encode(uint8_t** data, int nb_samples);
	AVCodecContext* GetCodecContext() { return codec_ctx; }
	void Close();

private:
	SwrContext* swr_ctx = nullptr;
	AVCodecContext* codec_ctx = nullptr;
	AVFrame* frame = nullptr;
	int64_t pts = 0;

	int dst_nb_samples = 0;
	int max_dst_nb_samples = 0;
	uint8_t** dst_data = nullptr;
};

