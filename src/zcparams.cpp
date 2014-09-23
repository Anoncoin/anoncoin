#include "zcparams.h"

namespace zc = libzerocoin;

zc::Params* GetZerocoinParams()
{
    static zc::Params params;
    return &params;
}
