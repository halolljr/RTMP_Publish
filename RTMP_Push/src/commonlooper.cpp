#include "commonlooper.h"
#include "dlog.h"

namespace LQF
{
void* CommonLooper::trampoline(void* p) {
    LogInfo("at CommonLooper trampoline");
    ((CommonLooper*)p)->Loop();
    return NULL;
}

CommonLooper::CommonLooper()
{
    request_exit_ = false;
}

RET_CODE CommonLooper::Start()
{
    LogInfo("at CommonLooper create");
    //把this作为参数传进去，又因为是静态函数，因此无需加取地址和签名
    //你也可以统一性写法：worker_ = new std::thread(&CommonLooper::trampoline, this);
    worker_ = new std::thread(trampoline, this);
    if(worker_ == NULL)
    {
        LogError("new std::thread failed");
        return RET_FAIL;
    }

    running_ = true;
    return RET_OK;
}


CommonLooper::~CommonLooper()
{
    if (running_)
    {
        LogInfo("CommonLooper deleted while still running. Some messages will not be processed");
        Stop();
    }
}


void CommonLooper::Stop()
{
    request_exit_ = true;
    if(worker_)
    {
        worker_->join();
        delete worker_;
        worker_ = NULL;
    }
    running_ = false;
}

}
