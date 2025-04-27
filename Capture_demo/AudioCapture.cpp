#include "AudioCapture.h"
#include <iostream>

AudioCapture::AudioCapture() {
	avdevice_register_all();
}

AudioCapture::~AudioCapture() {
	close();
}

bool AudioCapture::open(std::string device_name) {
	AVInputFormat* input_fmt = av_find_input_format("dshow");
	if (avformat_open_input(&fmt_ctx, device_name.c_str(), input_fmt, nullptr) != 0) {
		std::cerr << "Failed to open audio device\n";
		return false;
	}
	avformat_find_stream_info(fmt_ctx, nullptr);

	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			stream_index = i;
			break;
		}
	}

	AVCodec* decoder = avcodec_find_decoder(fmt_ctx->streams[stream_index]->codecpar->codec_id);
	dec_ctx = avcodec_alloc_context3(decoder);
	avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[stream_index]->codecpar);
	avcodec_open2(dec_ctx, decoder, nullptr);

	swr_ctx = swr_alloc_set_opts(nullptr,
		AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLTP, 44100,
		av_get_default_channel_layout(dec_ctx->channels), (AVSampleFormat)dec_ctx->sample_fmt, dec_ctx->sample_rate,
		0, nullptr);
	if (swr_init(swr_ctx) < 0) {
		std::cerr << "Failed to initialize swr_ctx" << std::endl;
		return false;
	}

	pcm_frame = av_frame_alloc();
	pcm_frame->format = AV_SAMPLE_FMT_FLTP;
	pcm_frame->channel_layout = AV_CH_LAYOUT_STEREO;
	pcm_frame->sample_rate = 44100;
	pcm_frame->nb_samples = 1024;
	av_frame_get_buffer(pcm_frame, 0);

	return true;
}

void AudioCapture::close() {
	if (pcm_frame) av_frame_free(&pcm_frame);
	if (swr_ctx) swr_free(&swr_ctx);
	if (dec_ctx) avcodec_free_context(&dec_ctx);
	if (fmt_ctx) avformat_close_input(&fmt_ctx);
}

AVFrame* AudioCapture::captureFrame() {
	AVPacket pkt;
	av_init_packet(&pkt);
	if (av_read_frame(fmt_ctx, &pkt) >= 0 && pkt.stream_index == stream_index) {
		if (avcodec_send_packet(dec_ctx, &pkt) == 0) {
			AVFrame* frame = av_frame_alloc();
			if (avcodec_receive_frame(dec_ctx, frame) == 0) {
				swr_convert(swr_ctx, pcm_frame->data, pcm_frame->nb_samples,
					(const uint8_t**)frame->data, frame->nb_samples);
				// 深拷贝一份pcm_frame
				AVFrame* new_frame = av_frame_alloc();
				if (!new_frame) {
					av_frame_free(&frame);
					av_packet_unref(&pkt);
					return nullptr;
				}

				// 设置格式信息
				new_frame->channels = pcm_frame->channels;
				new_frame->channel_layout = pcm_frame->channel_layout;
				new_frame->format = pcm_frame->format;  // 注意一定要设 format！
				new_frame->sample_rate = pcm_frame->sample_rate;
				new_frame->nb_samples = pcm_frame->nb_samples;

				// 分配内存
				av_samples_alloc(new_frame->data, new_frame->linesize,
					new_frame->channels, new_frame->nb_samples,
					(AVSampleFormat)new_frame->format, 0);

				// 拷贝数据
				av_samples_copy(new_frame->data, pcm_frame->data,
					0, 0, pcm_frame->nb_samples, pcm_frame->channels, (AVSampleFormat)new_frame->format);

				// 拷贝时间戳等
				av_frame_copy_props(new_frame, pcm_frame);
				// 清理
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
