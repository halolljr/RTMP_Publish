#include "VideoEncoder.h"
bool VideoEncoder::Open(int width, int height, int fps, AVPixelFormat in_pix_fmt) {
	const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	codec_ctx = avcodec_alloc_context3(codec);

	codec_ctx->width = width;
	codec_ctx->height = height;
	codec_ctx->time_base = AVRational{ 1, fps };
	codec_ctx->framerate = AVRational{ fps, 1 };
	codec_ctx->pix_fmt = in_pix_fmt;
	codec_ctx->gop_size = 12;
	codec_ctx->max_b_frames = 0;
	av_opt_set(codec_ctx->priv_data, "preset", "ultrafast", 0);

	return avcodec_open2(codec_ctx, codec, nullptr) >= 0;
}

AVPacket* VideoEncoder::Encode(AVFrame* frame) {
	frame->pts = pts++;
	avcodec_send_frame(codec_ctx, frame);
	AVPacket* pkt = av_packet_alloc();
	if (avcodec_receive_packet(codec_ctx, pkt) == 0)
		return pkt;

	av_packet_free(&pkt);
	return nullptr;
}

void VideoEncoder::Close() {
	if (codec_ctx) avcodec_free_context(&codec_ctx);
}

