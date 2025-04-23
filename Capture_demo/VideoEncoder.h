#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}
class VideoEncoder {
public:
	bool Open(int width, int height, int fps, AVPixelFormat in_pix_fmt = AV_PIX_FMT_YUV420P);
	AVPacket* Encode(AVFrame* frame); //  ‰»ÎYUV÷°£¨ ‰≥ˆ—πÀı∞¸
	AVCodecContext* GetCodecContext() { return codec_ctx; }
	void Close();

private:
	AVCodecContext* codec_ctx = nullptr;
	AVFrame* frame = nullptr;
	int64_t pts = 0;
};