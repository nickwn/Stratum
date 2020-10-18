#pragma once

#include "../Stratum.hpp"

namespace stm {

struct ProfilerSample {
	std::string mLabel;
	ProfilerSample* mParent;
	std::list<ProfilerSample*> mChildren;
	std::chrono::high_resolution_clock::time_point mStartTime;
	std::chrono::nanoseconds mDuration;
	float4 mColor;
	inline ~ProfilerSample() { for (ProfilerSample* c : mChildren) delete c; }
};

class Profiler {
public:

	STRATUM_API static void BeginSample(const std::string& label);
	STRATUM_API static void EndSample();

	STRATUM_API static void DrawGui(GuiContext& gui, uint32_t framerate);
	STRATUM_API static void ClearAll();

private:
	friend class Instance;
	STRATUM_API static void BeginFrame(uint64_t frameIndex);
	STRATUM_API static void EndFrame();

	STRATUM_API static bool mEnabled;
	STRATUM_API static std::list<ProfilerSample*> mFrames;
	STRATUM_API static ProfilerSample* mCurrentSample;
	STRATUM_API static uint32_t mHistoryCount;
	STRATUM_API static const std::chrono::high_resolution_clock mTimer;

	// Drawing settings
	
	STRATUM_API static ProfilerSample* mSelectedFrame;
	STRATUM_API static float mGraphHeight;
	STRATUM_API static float mSampleHeight;
};

}