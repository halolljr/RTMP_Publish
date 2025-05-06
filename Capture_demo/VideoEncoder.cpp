#include "VideoEncoder.h"
#include <iostream>

VideoEncoder::VideoEncoder() {}

VideoEncoder::~VideoEncoder() {
	//close();
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
	enc_ctx->qmin = 10;
	enc_ctx->qmax = 31;
	av_dict_set(&param, "preset", "fast", 0);
	av_dict_set(&param, "tune", "zerolatency", 0);
	if (avcodec_open2(enc_ctx, encoder, &param) != 0) {
		std::cerr << "Failed to open H264 encoder" << std::endl;
		return false;
	}

	return true;
}

void VideoEncoder::close() {
	if (enc_ctx) avcodec_free_context(&enc_ctx);
	if (param) av_dict_free(&param);
}

bool VideoEncoder::encode(AVFrame* frame,AVPacket* pkt) {
	int ret = avcodec_send_frame(enc_ctx, frame);
	if (ret < 0 && ret != AVERROR(EAGAIN)) {
		return false;
	}

	ret = avcodec_receive_packet(enc_ctx, pkt);
	if (ret == 0) {
		return true;
	}

	return false;
}
