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
	AVPacket* encode(AVFrame* frame); // 编码一帧，返回编码后的Packet（需要释放）

private:
	AVCodecContext* enc_ctx = nullptr;
	AVFrame* tmp_frame = nullptr;
	int frame_pts = 0;
};