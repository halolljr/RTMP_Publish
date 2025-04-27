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
	AVPacket* encode(AVFrame* frame); // ����һ֡�����ر�����Packet����Ҫ�ͷţ�

private:
	AVCodecContext* enc_ctx = nullptr;
	AVFrame* tmp_frame = nullptr;
	int frame_pts = 0;
};