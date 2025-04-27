#include "VideoEncoder.h"
#include <iostream>

VideoEncoder::VideoEncoder() {}

VideoEncoder::~VideoEncoder() {
	close();
}

bool VideoEncoder::open(int width, int height, int fps, int bitrate) {
	AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder) {
		std::cerr << "H264 encoder not found" << std::endl;
		return false;
	}

	enc_ctx = avcodec_alloc_context3(encoder);
	enc_ctx->width = width;
	enc_ctx->height = height;
	enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	enc_ctx->time_base = { 1, fps };
	enc_ctx->framerate = { fps, 1 };
	enc_ctx->bit_rate = bitrate;
	enc_ctx->gop_size = fps; // 1ÃëÒ»¸öIÖ¡
	enc_ctx->max_b_frames = 0;

	av_opt_set(enc_ctx->priv_data, "preset", "ultrafast", 0);

	if (avcodec_open2(enc_ctx, encoder, nullptr) != 0) {
		std::cerr << "Failed to open H264 encoder" << std::endl;
		return false;
	}

	tmp_frame = av_frame_alloc();
	tmp_frame->format = enc_ctx->pix_fmt;
	tmp_frame->width = width;
	tmp_frame->height = height;
	av_frame_get_buffer(tmp_frame, 0);

	return true;
}

void VideoEncoder::close() {
	if (tmp_frame) av_frame_free(&tmp_frame);
	if (enc_ctx) avcodec_free_context(&enc_ctx);
}

AVPacket* VideoEncoder::encode(AVFrame* frame) {
	frame->pts = frame_pts++;

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
