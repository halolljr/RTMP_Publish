#include "Muxer.h"

Muxer::Muxer()
{

}

Muxer::~Muxer()
{
}

bool Muxer::Open(const char* file_name, AVCodecContext* video_ctx, AVCodecContext* audio_ctx)
{
	out_put_filename = file_name;
	if (avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, file_name) < 0) {
		std::cerr << "Failed to allocate output context!" << std::endl;
		return false;
	}
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
		video_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
		audio_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	// 创建视频流
	video_st = avformat_new_stream(ofmt_ctx, nullptr);
	if (!video_st) {
		std::cerr << "Failed to create video stream!" << std::endl;
		return false;
	}
	avcodec_parameters_from_context(video_st->codecpar, video_ctx);

	// 创建音频流
	audio_st = avformat_new_stream(ofmt_ctx, nullptr);
	if (!audio_st) {
		std::cerr << "Failed to create audio stream!" << std::endl;
		return false;
	}
	avcodec_parameters_from_context(audio_st->codecpar, audio_ctx);

	// 打开输出文件
	if (avio_open(&ofmt_ctx->pb, file_name, AVIO_FLAG_READ_WRITE) < 0) {
		std::cerr << "Failed to open output file!" << std::endl;
		return false;
	}

	// 打印文件信息
	av_dump_format(ofmt_ctx, 0, file_name, 1);

	// 写入文件头部
	if (avformat_write_header(ofmt_ctx, nullptr) < 0) {
		std::cerr << "Failed to write output file header!" << std::endl;
		return false;
	}

	return true;
}

void Muxer::Close()
{
	if (ofmt_ctx) {
		if (video_st || audio_st) {
			av_write_trailer(ofmt_ctx);
		}
	}
	if (ofmt_ctx) {
		avio_closep(&ofmt_ctx->pb);
		// 释放输出格式上下文
		avformat_free_context(ofmt_ctx);
		ofmt_ctx = nullptr;  // 防止悬空指针
	}
}

int Muxer::sendPacket(AVPacket* pkt)
{
	int ret = av_interleaved_write_frame(ofmt_ctx, pkt);
	if (ret < 0) {
		char tmpErrString[128] = { 0 };
		std::cerr << "Coudl not write video frame,error : " << av_make_error_string(tmpErrString, AV_ERROR_MAX_STRING_SIZE, ret) << std::endl;
		return ret;
	}
	return 0;
}
