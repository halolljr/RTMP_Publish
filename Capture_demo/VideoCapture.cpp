#include "VideoCapture.h"
#include <iostream>

VideoCapture::VideoCapture() {
	avdevice_register_all();
}

VideoCapture::~VideoCapture() {
	Close();
}

void VideoCapture::Start()
{
	running_ = true;
	capture_thread_ = new std::thread(&VideoCapture::captureFrame, this);
}

void VideoCapture::Stop()
{
	running_ = false;
}

void VideoCapture::Restart()
{
	running_ = true;
}

bool VideoCapture::Open(std::string device_name) {
	if (device_name.empty()) {
		std::cerr << "Invalid Video device_name" << std::endl;
		return false;
	}
	AVInputFormat* input_fmt = av_find_input_format("dshow");
	if (avformat_open_input(&fmt_ctx, device_name.c_str(), input_fmt, &options) != 0) {
		std::cerr << "Failed to open video device" << std::endl;
		return false;
	}
	avformat_find_stream_info(fmt_ctx, nullptr);

	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			stream_index_ = i;
			break;
		}
	}

	AVCodec* decoder = avcodec_find_decoder(fmt_ctx->streams[stream_index_]->codecpar->codec_id);
	dec_ctx = avcodec_alloc_context3(decoder);
	avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[stream_index_]->codecpar);
	avcodec_open2(dec_ctx, decoder, nullptr);

	if (dec_pkt == nullptr) {
		dec_pkt = av_packet_alloc();
	}
	if (dec_frame == nullptr) {
		dec_frame = av_frame_alloc();
	}
	//yuv_frame = av_frame_alloc();
	//yuv_frame->format = AV_PIX_FMT_YUV420P;
	//yuv_frame->width = dec_ctx->width;
	//yuv_frame->height = dec_ctx->height;
	//av_frame_get_buffer(yuv_frame, 0);

	return true;
}

void VideoCapture::Close() {
	//if (yuv_frame) av_frame_free(&yuv_frame);
	Stop();
	if (capture_thread_->joinable()) {
		capture_thread_->join();
	}
	if (dec_pkt) av_packet_free(&dec_pkt);
	if (dec_frame) av_frame_free(&dec_frame);
	if (dec_ctx) avcodec_free_context(&dec_ctx);
	if (fmt_ctx) avformat_close_input(&fmt_ctx);
	if(options)	av_dict_free(&options);
}

int VideoCapture::captureFrame() {
	while (running_) {
		if (!running_) {
			break;
		}
		if (av_read_frame(fmt_ctx, dec_pkt) >= 0 && dec_pkt->stream_index == stream_index_) {
			if (avcodec_send_packet(dec_ctx, dec_pkt) == 0) {
				if (avcodec_receive_frame(dec_ctx, dec_frame) == 0) {
					if (call_back) {
						//std::cout << "[Video-Frame]:" << dec_frame->pkt_size << std::endl;
						call_back(fmt_ctx->streams[stream_index_],(enum AVPixelFormat)fmt_ctx->streams[stream_index_]->codecpar->format, dec_frame, av_gettime());
					}
					av_frame_unref(dec_frame);
				}
			}
			av_packet_unref(dec_pkt);
		}
	}
	return 0;
}
