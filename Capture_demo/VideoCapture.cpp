#include "VideoCapture.h"
bool VideoCapture::Open(const std::string& device) {
	AVInputFormat* ifmt = av_find_input_format("dshow");
	if (avformat_open_input(&fmt_ctx, device.c_str(), ifmt, nullptr) < 0)
		return false;

	if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
		return false;

	for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_index = i;
			break;
		}
	}
	if (video_stream_index < 0) return false;

	AVCodec* codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_index]->codecpar->codec_id);
	codec_ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
	if (avcodec_open2(codec_ctx, codec, nullptr) < 0) return false;

	frame = av_frame_alloc();
	pkt = av_packet_alloc();
	return true;
}

void VideoCapture::SetCallback(std::function<void(uint8_t* data[], int linesize[], int width, int height, int64_t pts)> cb) {
	callback = std::move(cb);
}

void VideoCapture::Start() {
	running = true;
	thread = std::thread(&VideoCapture::CaptureLoop, this);
}

void VideoCapture::Stop() {
	running = false;
	if (thread.joinable()) thread.join();
	if (pkt) av_packet_free(&pkt);
	if (frame) av_frame_free(&frame);
	if (codec_ctx) avcodec_free_context(&codec_ctx);
	if (fmt_ctx) avformat_close_input(&fmt_ctx);
}

void VideoCapture::CaptureLoop() {
	while (running) {
		if (av_read_frame(fmt_ctx, pkt) >= 0 && pkt->stream_index == video_stream_index) {
			if (avcodec_send_packet(codec_ctx, pkt) == 0) {
				while (avcodec_receive_frame(codec_ctx, frame) == 0) {
					if (callback) {
						callback(frame->data, frame->linesize, frame->width, frame->height, frame->pts);
					}
				}
			}
			av_packet_unref(pkt);
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	}
}

int VideoCapture::GetWidth() const {
	return codec_ctx ? codec_ctx->width : 640;
}

int VideoCapture::GetHeight() const {
	return codec_ctx ? codec_ctx->height : 480;
}

int VideoCapture::GetFPS() const {
	return 25;
}

AVPixelFormat VideoCapture::GetPixelFormat() const {
	return codec_ctx ? codec_ctx->pix_fmt : AV_PIX_FMT_YUYV422;
}
