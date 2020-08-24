#pragma once

#include <Util/Util.hpp>

class Bone;
typedef std::vector<Bone*> AnimationRig;

struct BoneTransform {
	float3 mPosition;
	quaternion mRotation;
	float3 mScale;

	inline BoneTransform operator*(const BoneTransform& rhs) const {
		BoneTransform t = {};
		t.mPosition = mPosition + (mRotation * rhs.mPosition) * mScale;
		t.mRotation = mRotation * rhs.mRotation;
		t.mScale = rhs.mScale * mScale;
		return t;
	}
};

typedef std::vector<BoneTransform> Pose;

inline BoneTransform inverse(const BoneTransform& bt) {
	BoneTransform t = {};
	t.mRotation = inverse(bt.mRotation);
	t.mPosition = (t.mRotation * -bt.mPosition) / bt.mScale;
	t.mScale = 1.f / bt.mScale;
	return t;
}

inline BoneTransform lerp(const BoneTransform& p0, const BoneTransform& p1, float t) {
	BoneTransform dest;
	dest.mPosition = lerp(p0.mPosition, p1.mPosition, t);
	dest.mRotation = slerp(p0.mRotation, p1.mRotation, t);
	dest.mScale = lerp(p0.mScale, p1.mScale, t);
	return dest;
}
inline void lerp(Pose& dest, const Pose& p0, const Pose& p1, float t) {
	for (uint32_t i = 0; i < dest.size(); i++)
		dest[i] = lerp(p0[i], p1[i], t);
}

struct AnimationKeyframe {
	float mValue;
	float mTime;
	float mTangentIn;
	float mTangentOut;
	AnimationTangentMode mTangentModeIn;
	AnimationTangentMode mTangentModeOut;
};
class AnimationChannel {
public:
	inline AnimationChannel() {};
	STRATUM_API AnimationChannel(const std::vector<AnimationKeyframe>& keyframes, AnimationExtrapolateMode in, AnimationExtrapolateMode out);
	STRATUM_API float Sample(float t) const;

	inline AnimationExtrapolateMode ExtrapolateIn() const { return mExtrapolateIn; }
	inline AnimationExtrapolateMode ExtrapolateOut() const { return mExtrapolateOut; }
	inline uint32_t KeyframeCount() const { return (uint32_t)mKeyframes.size(); }
	inline AnimationKeyframe Keyframe(uint32_t index) const { return mKeyframes[index]; }
	inline float4 CurveCoefficient(uint32_t index) const { return mCoefficients[index]; }

private:
	AnimationExtrapolateMode mExtrapolateIn   = AnimationExtrapolateMode::eConstant;
	AnimationExtrapolateMode mExtrapolateOut  = AnimationExtrapolateMode::eConstant;
	std::vector<float4> mCoefficients;
	std::vector<AnimationKeyframe> mKeyframes;
};

class Animation {
public:
	STRATUM_API Animation(const std::unordered_map<uint32_t, AnimationChannel>& channels, float start, float end);

	inline const float TimeStart() const { return mTimeStart; }
	inline const float TimeEnd() const { return mTimeEnd; }
	inline const std::unordered_map<uint32_t, AnimationChannel>& Channels() const { return mChannels; }

	STRATUM_API void Sample(float t, AnimationRig& rig) const;

private:
	std::unordered_map<uint32_t, AnimationChannel> mChannels;
	float mTimeStart;
	float mTimeEnd;
};