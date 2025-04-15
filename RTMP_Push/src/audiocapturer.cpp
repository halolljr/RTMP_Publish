#include "audiocapturer.h"

#include "dlog.h"
#include "timeutil.h"
#include "avtimebase.h"
namespace LQF
{
AudioCapturer::AudioCapturer()
{

}

AudioCapturer::~AudioCapturer()
{
    Stop();
    if(pcm_buf_)
        delete [] pcm_buf_;
}

RET_CODE AudioCapturer::Init(const Properties &properties)
{
    audio_test_ = properties.GetProperty("audio_test", 0);
    input_pcm_name_ = properties.GetProperty("input_pcm_name", "buweishui_48000_2_s16le.pcm");
    sample_rate_ = properties.GetProperty("sample_rate", 48000);
    pcm_buf_ = new uint8_t[PCM_BUF_MAX_SIZE];
    if(!pcm_buf_)
    {
        return RET_ERR_OUTOFMEMORY;
    }
    if(openPcmFile(input_pcm_name_.c_str()) != 0)
    {
        LogError("openPcmFile %s failed", input_pcm_name_.c_str());
        return RET_FAIL;
    }
    return RET_OK;
}

void AudioCapturer::Loop()
{
    LogInfo("into loop");

    int nb_samples = 1024;

    pcm_total_duration_ = 0;
    pcm_start_time_ = TimesUtil::GetTimeMillisecond();
    LogInfo("into loop while");
    while(true)
    {
        if(request_exit_)
            break;
        if(readPcmFile(pcm_buf_, nb_samples) == 0)
        {
            if(!is_first_frame_) {
                is_first_frame_ = true;
                LogInfo("%s:t%u", AVPublishTime::GetInstance()->getAInTag(),
                        AVPublishTime::GetInstance()->getCurrenTime());
            }
            if(callback_get_pcm_)
            {
                callback_get_pcm_(pcm_buf_, nb_samples *4); // 2通道，源数据s16格式是2字节[fltp作为编码是4字节]
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    closePcmFile();
}

int AudioCapturer::openPcmFile(const char *file_name)
{
    pcm_fp_ = fopen(file_name, "rb");
    if(!pcm_fp_)
    {
        return -1;
    }
    return 0;
}

int AudioCapturer::readPcmFile(uint8_t *pcm_buf, int32_t nb_samples)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();
    int64_t dif = cur_time - pcm_start_time_;
//    printf("%lld, %lld\n", pcm_total_duration_, dif);
    //音视频改进的地方，可能会导致不同步
    if((int64_t)pcm_total_duration_ > dif)
        return -1;
    //假设是 2通道、原数据16e是16位音频（每通道 2 字节，2*2 = 4 字节每帧）
    //一次读取每帧读取的总字节数 = nb_samples * 4
    size_t ret = ::fread(pcm_buf, 1, nb_samples *4, pcm_fp_);
    //模拟循环播放PCM文件
    if(ret != nb_samples *4)
    {
        // 从文件头部开始读取
        ret = ::fseek(pcm_fp_, 0, SEEK_SET);
        ret = ::fread(pcm_buf, 1, nb_samples *4, pcm_fp_);
        if(ret != nb_samples *4)
        {
            return -1;
        }
    }
//    LogInfo("pcm_total_duration_:%lldms", (int64_t)pcm_total_duration_);
    //除以48而不是48000是因为从系统获取到的时间是毫秒级别，所以这里用来比对的也是毫秒级别
    pcm_total_duration_ +=(nb_samples*1.0/48);
    return 0;
}

int AudioCapturer::closePcmFile()
{
    if(pcm_fp_)
        fclose(pcm_fp_);
    return 0;
}
}
