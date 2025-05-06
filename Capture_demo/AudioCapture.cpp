#include "AudioCapture.h"
#include <iostream>

AudioCapture::AudioCapture() {
	avdevice_register_all();
}

AudioCapture::~AudioCapture() {
	//Close();
}

bool AudioCapture::Open(std::string device_name) {
	if (device_name.empty()) {
		std::cerr << "Invalid Audio device_name" << std::endl;
		return false;
	}
	AVInputFormat* input_fmt = av_find_input_format("dshow");
	if (avformat_open_input(&fmt_ctx, device_name.c_str(), input_fmt, NULL) != 0) {
		std::cerr << "Failed to open audio device\n";
		return false;
	}
	avformat_find_stream_info(fmt_ctx, nullptr);

	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
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
		if (nullptr == dec_pkt) {
			std::cerr << "Could not av_packet_alloc()" << std::endl;
			return false;
		}
	}
	if (dec_frame == nullptr) {
		dec_frame = av_frame_alloc();
		if (nullptr == dec_frame) {
			std::cerr << "Could not av_frame_alloc()" << std::endl;
			return false;
		}
	}
	//swr_ctx = swr_alloc_set_opts(nullptr,
	//	AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLTP, 44100,
	//	av_get_default_channel_layout(dec_ctx->channels), (AVSampleFormat)dec_ctx->sample_fmt, dec_ctx->sample_rate,
	//	0, nullptr);
	//if (swr_init(swr_ctx) < 0) {
	//	std::cerr << "Failed to initialize swr_ctx" << std::endl;
	//	return false;
	//}

	//pcm_frame = av_frame_alloc();
	//pcm_frame->format = AV_SAMPLE_FMT_FLTP;
	//pcm_frame->channel_layout = AV_CH_LAYOUT_STEREO;
	//pcm_frame->sample_rate = 44100;
	//pcm_frame->nb_samples = 1024;
	//av_frame_get_buffer(pcm_frame, 0);
	av_dump_format(fmt_ctx, 0, device_name.c_str(), 0);
	return true;
}

void AudioCapture::Start()
{
	running_ = true;
	capture_thread_ = new std::thread(&AudioCapture::captureFrame, this);
}

void AudioCapture::Stop()
{
	running_ = false;
}

void AudioCapture::Restart()
{
	running_ = true;
}

void AudioCapture::Close() {
	Stop();
	if (nullptr != capture_thread_) {
		if (capture_thread_->joinable()) {
			capture_thread_->join();
		}
	}
	if (dec_pkt) av_packet_free(&dec_pkt);
	if (dec_frame) av_frame_free(&dec_frame);
	//if (pcm_frame) av_frame_free(&pcm_frame);
	if (dec_ctx) avcodec_free_context(&dec_ctx);
	if (fmt_ctx) avformat_close_input(&fmt_ctx);
}

int AudioCapture::captureFrame() {
	
	while (running_) {
		if (!running_) {
			break;
		}
		if (av_read_frame(fmt_ctx, dec_pkt) >= 0 && dec_pkt->stream_index == stream_index_) {
			if (avcodec_send_packet(dec_ctx, dec_pkt) == 0) {
				if (avcodec_receive_frame(dec_ctx, dec_frame) == 0) {
					if (call_back) {
						//std::cout << "[Audio-Frame]:" << dec_frame->pkt_size << std::endl;
						call_back(fmt_ctx->streams[stream_index_], dec_frame,av_gettime());
					}
					av_frame_unref(dec_frame);
				}
			}
			av_packet_unref(dec_pkt);
		}
	}
	return 0;
}
