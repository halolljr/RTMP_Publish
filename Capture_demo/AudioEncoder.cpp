#include "AudioEncoder.h"
#include <iostream>

AudioEncoder::AudioEncoder() {}

AudioEncoder::~AudioEncoder() {
	close();
}

bool AudioEncoder::open(int sample_rate, int channels, int bitrate) {
	AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!encoder) {
		std::cerr << "AAC encoder not found" << std::endl;
		return false;
	}

	enc_ctx = avcodec_alloc_context3(encoder);
	enc_ctx->sample_rate = sample_rate;
	enc_ctx->channel_layout = channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
	enc_ctx->channels = channels;
	enc_ctx->bit_rate = bitrate;
	enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // Í¨³£ÊÇAV_SAMPLE_FMT_FLTP
	enc_ctx->time_base = { 1, sample_rate };

	if (avcodec_open2(enc_ctx, encoder, nullptr) != 0) {
		std::cerr << "Failed to open AAC encoder" << std::endl;
		return false;
	}

	return true;
}

void AudioEncoder::close() {
	if (enc_ctx) avcodec_free_context(&enc_ctx);
}

AVPacket* AudioEncoder::encode(AVFrame* frame) {
	frame->pts = pts;
	pts += frame->nb_samples;

	if (avcodec_send_frame(enc_ctx, frame) != 0) {
		return nullptr;
	}

	AVPacket* pkt = av_packet_alloc();
	if (avcodec_receive_packet(enc_ctx, pkt) == 0) {
		return pkt;
	}
	av_packet_free(&pkt);
	return nullptr;
}
