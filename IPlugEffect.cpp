#include "IPlugEffect.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <iostream>

class MyKnobWithMenu : public IVKnobControl
{
public:
  MyKnobWithMenu(const IRECT& bounds, int paramIdx, const char* label = "")
    : IVKnobControl(bounds, paramIdx, label)
  {
  }
  
  void CreateContextMenu(IPopupMenu& menu) override
  {
      menu.AddItem("1/2");
      menu.AddItem("1/3");
      menu.AddItem("1/4");
      menu.AddItem("1/6");
      menu.AddItem("1/8");
      menu.AddItem("1/12");
      menu.AddItem("1/16");
      menu.AddItem("1/24");
      menu.AddItem("1/32");
  }

  void OnContextSelection(int valIdx) override
  {
    auto* pPlugin = dynamic_cast<IPlugEffect*>(GetDelegate());

    auto bpm = pPlugin->GetTempo();
    int numerator, denominator;
    pPlugin->GetTimeSig(numerator, denominator);
    double msPerBar = (60000.0 / bpm) * numerator;

    double numerator2;
    switch (valIdx)
    {
    case 0:
      numerator2 = 2;
      break;
    case 1:
      numerator2 = 3;
      break;
    case 2:
      numerator2 = 4;
      break;
    case 3:
      numerator2 = 6;
      break;
    case 4:
      numerator2 = 8;
      break;
    case 5:
      numerator2 = 12;
      break;
    case 6:
      numerator2 = 16;
      break;
    case 7:
      numerator2 = 24;
      break;
    case 8:
      numerator2 = 32;
      break;
    }

    pPlugin->GetParam(kDelayTimeMs)->Set(msPerBar / numerator2);
    auto value = pPlugin->GetParam(kDelayTimeMs)->GetNormalized();
    SetValue(value);
    SetDirty(false);
  }
 };

IPlugEffect::IPlugEffect(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kDelayTimeMs)->InitDouble("Delay", 500.0, 1.0, 2000.0, 1.0, "ms", IParam::EFlags::kFlagStepped);
  GetParam(kFeedback)->InitDouble("Feedback", 0.5, 0.0, 0.99, 0.01, "");

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    const IRECT b = pGraphics->GetBounds();

    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    int knobSize = 80;
    int padding = 10; // Abstand zwischen den Knobs und zum Rand
    float startX = (b.W() - (2 * knobSize + padding)) / 2.0;
    float startY = 10; // Position von oben
    IRECT knob1Bounds(startX, startY, startX + knobSize, startY + knobSize);
    // Berechnung der Position für den zweiten Knob
    IRECT knob2Bounds(startX + knobSize + padding, startY, startX + knobSize + padding + knobSize, startY + knobSize);
    GetUI()->AttachControl(new MyKnobWithMenu(knob1Bounds, kDelayTimeMs));
    GetUI()->AttachControl(new IVKnobControl(knob2Bounds, kFeedback));
  };
#endif
}

#if IPLUG_DSP
void IPlugEffect::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const double sampleRate = GetSampleRate();
  const int nChannels = 2; // Typically 2 for stereo

  // Make sure delay buffers are correctly sized on reset or sample rate change
  if (m_bufferSize != static_cast<int>(2.1 * sampleRate)) // 2.1 seconds max delay time
  {
    m_bufferSize = static_cast<int>(2.1 * sampleRate);
    m_delayBuffer[0].resize(m_bufferSize, 0.0);
    m_delayBuffer[1].resize(m_bufferSize, 0.0);
    // Also reset write pointers and filters
    m_writePointer[0] = 0;
    m_writePointer[1] = 0;
  }

  double currentDelayTimeMs = GetParam(kDelayTimeMs)->Value();
  double currentFeedback = GetParam(kFeedback)->Value();

  for (int s = 0; s < nFrames; ++s)
  {
    for (int c = 0; c < nChannels; ++c)
    {
      const double inputSample = inputs[c][s];
      double delaySample = 0.0;

      // Calculate delay in samples (float to allow for fractional delay)
      double numDelaySamples = currentDelayTimeMs * (sampleRate / 1000.0);

      // Calculate read pointer (fractional)
      double readPointer = (double)m_writePointer[c] - numDelaySamples;

      // Handle wrap-around for read pointer
      while (readPointer < 0)
      {
        readPointer += m_bufferSize;
      }

      // Get integer and fractional parts for interpolation
      int iReadPointer = static_cast<int>(readPointer);
      double frac = readPointer - iReadPointer;

      // Get two samples for linear interpolation
      double sample1 = m_delayBuffer[c][iReadPointer];
      int iReadPointerNext = (iReadPointer + 1) % m_bufferSize; // Wrap around for next sample
      double sample2 = m_delayBuffer[c][iReadPointerNext];

      // Perform linear interpolation
      delaySample = LinearInterpolation(sample1, sample2, frac);

      double wetSignal = inputSample + delaySample * currentFeedback;

      // Write current signal (input + feedback) into delay buffer
      m_delayBuffer[c][m_writePointer[c]] = wetSignal;

      // Increment write pointer and wrap around
      m_writePointer[c] = (m_writePointer[c] + 1) % m_bufferSize;

      outputs[c][s] = wetSignal;
    }
  }
}
double IPlugEffect::LinearInterpolation(double y1, double y2, double frac) { return y1 + frac * (y2 - y1); }
#endif
