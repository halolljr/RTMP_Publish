#include "AudioCapture.h"
bool AudioCapture::Open(const std::string& device) {
	AVInputFormat* ifmt = av_find_input_format("dshow");
	if (avformat_open_input(&fmt_ctx, device.c_str(), ifmt, nullptr) < 0)
		return false;

	if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
		return false;

	for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_stream_index = i;
			break;
		}
	}
	codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
	pkt = av_packet_alloc();
	return true;
}

void AudioCapture::SetCallback(std::function<void(uint8_t* data, int size, int64_t pts)> cb) {
	callback = std::move(cb);
}

void AudioCapture::Start() {
	running = true;
	thread = std::thread(&AudioCapture::CaptureLoop, this);
}

void AudioCapture::Stop() {
	running = false;
	if (thread.joinable()) thread.join();
	if (pkt) av_packet_free(&pkt);
	if (fmt_ctx) avformat_close_input(&fmt_ctx);
}

void AudioCapture::CaptureLoop() {
	while (running) {
		if (av_read_frame(fmt_ctx, pkt) >= 0) {
			if (callback) {
				callback(pkt->data, pkt->size, pkt->pts);
			}
			av_packet_unref(pkt);
		}
		else {
			//av_usleep(1000);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	}
}

int AudioCapture::GetSampleRate() const {
	return codecpar ? codecpar->sample_rate : 44100;
}

int AudioCapture::GetChannels() const {
	return codecpar ? codecpar->channels : 2;
}

AVSampleFormat AudioCapture::GetSampleFormat() const {
	return AV_SAMPLE_FMT_S16;  // 一般 dshow 是 s16 格式
}
