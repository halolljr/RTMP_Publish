#include "AudioCapture.h"
#include "VideoCapture.h"
#include "AudioEncoder.h"
#include "VideoEncoder.h"
#include "MP4Muxer.h"

// 模拟 PushWork 结构
class PushWork {
public:
	virtual void OnYUVFrame(uint8_t* data[], int linesize[], int width, int height, int64_t pts) = 0;
	virtual void OnPCMFrame(uint8_t* data, int size, int64_t pts) = 0;
};

// 整合所有模块的工作类
class AVPushApp : public PushWork {
public:
	bool Init();
	void OnYUVFrame(uint8_t* data[], int linesize[], int width, int height, int64_t pts) override;
	void OnPCMFrame(uint8_t* data, int size, int64_t pts) override;
	void Run();

private:
	AudioCapture audio_capture;
	VideoCapture video_capture;
	AudioEncoder audio_encoder;
	VideoEncoder video_encoder;
	MP4Muxer mp4_muxer;

	SwsContext* sws_ctx = nullptr;
	uint8_t* yuv_buffer[4] = { nullptr };
	int yuv_linesize[4];

	int64_t start_pts = 0;
};

bool AVPushApp::Init() {
	// 初始化采集
	if (!audio_capture.Open()) return false;
	if (!video_capture.Open()) return false;

	// 初始化编码器
	int width = video_capture.GetWidth();
	int height = video_capture.GetHeight();
	int fps = video_capture.GetFPS();

	if (!video_encoder.Open(width, height, fps)) return false;

	if (!audio_encoder.Open(
		audio_capture.GetSampleRate(),
		audio_capture.GetChannels(),
		audio_capture.GetSampleFormat())) return false;

	// MP4封装器
	if (!mp4_muxer.Open("output.mp4", video_encoder.GetCodecContext(), audio_encoder.GetCodecContext()))
		return false;

	// 像素格式转换器
	sws_ctx = sws_getContext(width, height, video_capture.GetPixelFormat(),
		width, height, AV_PIX_FMT_YUV420P,
		SWS_BILINEAR, nullptr, nullptr, nullptr);

	av_image_alloc(yuv_buffer, yuv_linesize, width, height, AV_PIX_FMT_YUV420P, 1);

	return true;
}

void AVPushApp::OnYUVFrame(uint8_t* data[], int linesize[], int width, int height, int64_t pts) {
	// 创建输入 AVFrame
	AVFrame* frame = av_frame_alloc();
	frame->format = AV_PIX_FMT_YUV420P;
	frame->width = width;
	frame->height = height;

	// 转换到YUV420P
	sws_scale(sws_ctx, data, linesize, 0, height, yuv_buffer, yuv_linesize);
	av_image_fill_arrays(frame->data, frame->linesize, yuv_buffer[0], AV_PIX_FMT_YUV420P, width, height, 1);

	// 编码
	AVPacket* pkt = video_encoder.Encode(frame);
	if (pkt) mp4_muxer.WriteVideo(pkt);

	av_frame_free(&frame);
}

void AVPushApp::OnPCMFrame(uint8_t* data, int size, int64_t pts) {
	// 假设是 planar 数据，每帧为 1024 样本，2通道
	uint8_t* pcm_planes[2] = { data, data + size / 2 };
	AVPacket* pkt = audio_encoder.Encode(pcm_planes, 1024);
	if (pkt) mp4_muxer.WriteAudio(pkt);
}

void AVPushApp::Run() {
	audio_capture.SetCallback([this](uint8_t* data, int size, int64_t pts) {
		this->OnPCMFrame(data, size, pts);
		});

	video_capture.SetCallback([this](uint8_t* data[], int linesize[], int width, int height, int64_t pts) {
		this->OnYUVFrame(data, linesize, width, height, pts);
		});

	audio_capture.Start();
	video_capture.Start();

	// 简单等待主线程工作一段时间
	std::this_thread::sleep_for(std::chrono::seconds(60));

	audio_capture.Stop();
	video_capture.Stop();

	mp4_muxer.Close();
}

int main() {
	av_log_set_level(AV_LOG_ERROR);
	avdevice_register_all();

	AVPushApp app;
	if (!app.Init()) {
		printf("Init failed\n");
		return -1;
	}
	app.Run();
	printf("Finished.\n");
	return 0;
}
