#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;

enum EParams
{
  kDelayTimeMs = 0,
  kFeedback,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class IPlugEffect final : public Plugin
{
public:
  IPlugEffect(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif

  private:
    std::vector<double> m_delayBuffer[2];
    int m_bufferSize = 0;
    int m_writePointer[2] = {0, 0};

    double LinearInterpolation(double y1, double y2, double frac);
};
