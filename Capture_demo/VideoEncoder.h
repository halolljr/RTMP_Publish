#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}
class VideoEncoder {
public:
	VideoEncoder();
	~VideoEncoder();

	bool open(int width, int height, int fps, int bitrate);
	void close();
	bool encode(AVFrame* frame,AVPacket* pkt); 
	AVCodecContext* GetCodecContext(){
		return enc_ctx;
	}
	int GetWidth() {
		return enc_ctx->width;
	}
	int GetHeight() {
		return enc_ctx->height;
	}
	enum AVPixelFormat GetAVPixelFormat() {
		return enc_ctx->pix_fmt;
	}
private:
	AVDictionary* param = nullptr;
	AVCodecContext* enc_ctx = nullptr;
	int frame_pts = 0;
};