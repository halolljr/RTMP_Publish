#include "VideoCapture.h"
#include <iostream>

VideoCapture::VideoCapture() {
	avdevice_register_all();
}

VideoCapture::~VideoCapture() {
	close();
}

bool VideoCapture::open(std::string device_name) {
	AVInputFormat* input_fmt = av_find_input_format("dshow");
	if (avformat_open_input(&fmt_ctx, device_name.c_str(), input_fmt, nullptr) != 0) {
		std::cerr << "Failed to open video device" << std::endl;
		return false;
	}
	avformat_find_stream_info(fmt_ctx, nullptr);

	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			stream_index = i;
			break;
		}
	}

	AVCodec* decoder = avcodec_find_decoder(fmt_ctx->streams[stream_index]->codecpar->codec_id);
	dec_ctx = avcodec_alloc_context3(decoder);
	avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[stream_index]->codecpar);
	avcodec_open2(dec_ctx, decoder, nullptr);

	sws_ctx = sws_getContext(
		dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
		dec_ctx->width, dec_ctx->height, AV_PIX_FMT_YUV420P,
		SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (!sws_ctx) {
		std::cerr << "Failed to initialize sws_ctx" << std::endl;
		return false;
	}

	yuv_frame = av_frame_alloc();
	yuv_frame->format = AV_PIX_FMT_YUV420P;
	yuv_frame->width = dec_ctx->width;
	yuv_frame->height = dec_ctx->height;
	av_frame_get_buffer(yuv_frame, 0);

	return true;
}

void VideoCapture::close() {
	if (yuv_frame) av_frame_free(&yuv_frame);
	if (sws_ctx) sws_freeContext(sws_ctx);
	if (dec_ctx) avcodec_free_context(&dec_ctx);
	if (fmt_ctx) avformat_close_input(&fmt_ctx);
}

AVFrame* VideoCapture::captureFrame() {
	AVPacket pkt;
	av_init_packet(&pkt);
	if (av_read_frame(fmt_ctx, &pkt) >= 0 && pkt.stream_index == stream_index) {
		if (avcodec_send_packet(dec_ctx, &pkt) == 0) {
			AVFrame* frame = av_frame_alloc();
			if (avcodec_receive_frame(dec_ctx, frame) == 0) {
				sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
					yuv_frame->data, yuv_frame->linesize);
				// 深拷贝一份yuv_frame
				AVFrame* new_frame = av_frame_alloc();

				// 先设置基本属性
				new_frame->format = AV_PIX_FMT_YUV420P;
				new_frame->width = yuv_frame->width;
				new_frame->height = yuv_frame->height;

				// 分配buffer
				av_frame_get_buffer(new_frame, 32);
				av_frame_make_writable(new_frame);

				// 复制图像数据
				av_image_copy(new_frame->data, new_frame->linesize,
					(const uint8_t**)yuv_frame->data, yuv_frame->linesize,
					AV_PIX_FMT_YUV420P, yuv_frame->width, yuv_frame->height);

				// 拷贝时间戳等属性
				av_frame_copy_props(new_frame, yuv_frame);

				av_frame_free(&frame);
				av_packet_unref(&pkt);
				return new_frame;
			}
			av_frame_free(&frame);
		}
	}
	av_packet_unref(&pkt);
	return nullptr;
}
