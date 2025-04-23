#pragma once
#include <string>
extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
class MP4Muxer {
public:
	bool Open(const std::string& filename, AVCodecContext* video_ctx, AVCodecContext* audio_ctx);
	void WriteVideo(AVPacket* pkt);
	void WriteAudio(AVPacket* pkt);
	void Close();

private:
	AVFormatContext* fmt_ctx = nullptr;
	AVStream* video_stream = nullptr;
	AVStream* audio_stream = nullptr;
	int64_t start_time = 0;
	bool header_written = false;

	void WriteHeader();
};

