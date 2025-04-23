#include "MP4Muxer.h"
bool MP4Muxer::Open(const std::string& filename, AVCodecContext* video_ctx, AVCodecContext* audio_ctx) {
	avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, filename.c_str());
	if (!fmt_ctx) return false;

	// 添加视频流
	if (video_ctx) {
		video_stream = avformat_new_stream(fmt_ctx, nullptr);
		avcodec_parameters_from_context(video_stream->codecpar, video_ctx);
		video_stream->time_base = video_ctx->time_base;
	}

	// 添加音频流
	if (audio_ctx) {
		audio_stream = avformat_new_stream(fmt_ctx, nullptr);
		avcodec_parameters_from_context(audio_stream->codecpar, audio_ctx);
		audio_stream->time_base = audio_ctx->time_base;
	}

	// 打开输出文件
	if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		if (avio_open(&fmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0)
			return false;
	}

	return true;
}

void MP4Muxer::WriteHeader() {
	if (!header_written) {
		avformat_write_header(fmt_ctx, nullptr);
		header_written = true;
		start_time = av_gettime();  // us
	}
}

void MP4Muxer::WriteVideo(AVPacket* pkt) {
	if (!video_stream) return;
	WriteHeader();

	pkt->stream_index = video_stream->index;
	av_packet_rescale_ts(pkt, video_stream->time_base, video_stream->time_base);
	av_interleaved_write_frame(fmt_ctx, pkt);
	av_packet_unref(pkt);
}

void MP4Muxer::WriteAudio(AVPacket* pkt) {
	if (!audio_stream) return;
	WriteHeader();

	pkt->stream_index = audio_stream->index;
	av_packet_rescale_ts(pkt, audio_stream->time_base, audio_stream->time_base);
	av_interleaved_write_frame(fmt_ctx, pkt);
	av_packet_unref(pkt);
}

void MP4Muxer::Close() {
	if (fmt_ctx) {
		if (header_written) av_write_trailer(fmt_ctx);
		if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
			avio_closep(&fmt_ctx->pb);
		avformat_free_context(fmt_ctx);
		fmt_ctx = nullptr;
	}
}

