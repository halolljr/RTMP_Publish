#ifndef NALULOOP_H
#define NALULOOP_H

#include "looper.h"

namespace LQF
{
//在继承Looper的基础上，重写了addmsg，专注处理音视频帧类消息的缓存控制、智能丢弃
class NaluLoop : public Looper
{
public:
    NaluLoop(int queue_nalu_len);
private:
    virtual void addmsg(LooperMessage *msg, bool flush);
private:
    int max_nalu_;
};
}
#endif // NALULOOP_H
