#include "AudioEncoder.h"
bool AudioEncoder::Open(int in_sample_rate, int in_channels, AVSampleFormat in_sample_fmt,
	int out_sample_rate, int out_channels, AVSampleFormat out_sample_fmt) {
	const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	codec_ctx = avcodec_alloc_context3(codec);

	codec_ctx->sample_rate = out_sample_rate;
	codec_ctx->channels = out_channels;
	codec_ctx->channel_layout = av_get_default_channel_layout(out_channels);
	codec_ctx->sample_fmt = out_sample_fmt;
	codec_ctx->time_base = AVRational{ 1, out_sample_rate };
	codec_ctx->bit_rate = 128000;

	if (avcodec_open2(codec_ctx, codec, nullptr) < 0) return false;

	// ÖØ²ÉÑùÆ÷
	swr_ctx = swr_alloc_set_opts(nullptr,
		av_get_default_channel_layout(out_channels), out_sample_fmt, out_sample_rate,
		av_get_default_channel_layout(in_channels), in_sample_fmt, in_sample_rate,
		0, nullptr);
	swr_init(swr_ctx);

	max_dst_nb_samples = 1024;
	av_samples_alloc_array_and_samples(&dst_data, nullptr, out_channels,
		max_dst_nb_samples, out_sample_fmt, 0);

	frame = av_frame_alloc();
	frame->nb_samples = max_dst_nb_samples;
	frame->channel_layout = codec_ctx->channel_layout;
	frame->format = out_sample_fmt;
	frame->sample_rate = out_sample_rate;

	return true;
}

AVPacket* AudioEncoder::Encode(uint8_t** data, int nb_samples) {
	dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, codec_ctx->sample_rate) + nb_samples,
		codec_ctx->sample_rate, codec_ctx->sample_rate, AV_ROUND_UP);

	swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t**)data, nb_samples);

	frame->data[0] = dst_data[0];
	if (codec_ctx->channels > 1)
		frame->data[1] = dst_data[1];
	frame->pts = pts;
	frame->nb_samples = dst_nb_samples;
	pts += dst_nb_samples;

	avcodec_send_frame(codec_ctx, frame);
	AVPacket* pkt = av_packet_alloc();
	if (!pkt) {
		return nullptr;
	}
	if (avcodec_receive_packet(codec_ctx, pkt) == 0)
		return pkt;

	av_packet_free(&pkt);
	return nullptr;
}

void AudioEncoder::Close() {
	if (swr_ctx) swr_free(&swr_ctx);
	if (codec_ctx) avcodec_free_context(&codec_ctx);
	if (frame) av_frame_free(&frame);
	if (dst_data) av_freep(&dst_data[0]);
	av_freep(&dst_data);
}
