#pragma once

#include "CommandBuffer.hpp"
#include "SpirvModule.hpp"

namespace stm {

class Material {
private:
	vector<shared_ptr<SpirvModule>> mModules;
	unordered_map<string, uint32_t> mSpecializationConstants;
	unordered_map<string, vector<byte>> mPushConstants;
	unordered_map<string, unordered_map<uint32_t, stm::Descriptor>> mDescriptors;
	unordered_map<string, shared_ptr<Sampler>> mImmutableSamplers;
	vk::CullModeFlags mCullMode = vk::CullModeFlagBits::eBack;
	vk::PolygonMode mPolygonMode = vk::PolygonMode::eFill;
	bool mSampleShading = false;
	vk::PipelineDepthStencilStateCreateInfo mDepthStencilState = vk::PipelineDepthStencilStateCreateInfo({}, true, true, vk::CompareOp::eLessOrEqual);
	vector<vk::PipelineColorBlendAttachmentState> mBlendStates;

	unordered_map<size_t, shared_ptr<GraphicsPipeline>> mPipelines;
	unordered_map<const Pipeline*, unordered_map<uint32_t, shared_ptr<DescriptorSet>>> mDescriptorSets;

public:
	string mName;

	inline Material(const string& name, const vk::ArrayProxy<const shared_ptr<SpirvModule>>& modules) : mName(name), mModules(modules.begin(), modules.end()) {}
	template<convertible_to<SpirvModule>... Args>
	inline Material(const string& name, const shared_ptr<Args>&... args) : Material(name, { args... }) {}

	inline shared_ptr<SpirvModule> stage(vk::ShaderStageFlagBits stage) const { 
		auto it = ranges::find(mModules, stage, &SpirvModule::stage);
		if (it == mModules.end())
			return nullptr;
		else
			return *it;
	}

	inline void set_immutable_sampler(const string& name, const shared_ptr<Sampler>& sampler) {
		auto it = mImmutableSamplers.find(name);
		mImmutableSamplers.emplace(name, sampler);
		mPipelines.clear();
	}

	inline void cull_mode(const vk::CullModeFlags& v) { mCullMode = v; }
	inline const auto& cull_mode() const { return mCullMode; }

	inline void polygon_mode(const vk::PolygonMode& v) { mPolygonMode = v; }
	inline const auto& polygon_mode() const { return mPolygonMode; }

	inline void sample_shading(bool v) { mSampleShading = v; }
	inline const auto& sample_shading() const { return mSampleShading; }

	inline auto& depth_stencil() { return mDepthStencilState; }
	inline const auto& depth_stencil() const { return mDepthStencilState; }

	inline void specialization_constant(const string& name, uint32_t v) {
		mSpecializationConstants.emplace(name, v);
	}
	inline uint32_t specialization_constant(const string& name) const {
		auto it = mSpecializationConstants.find(name);
		if (it != mSpecializationConstants.end())
			return it->second;
		for (const auto& spirv : mModules)
			if (auto it2 = spirv->specialization_constants().find(name); it2 != spirv->specialization_constants().end())
				return it2->second.second; // defailt value
		throw invalid_argument("No specialization constant named " + name);
	}
	
	template<typename T = vector<byte>>
	inline void push_constant(const string& name, const T& v) {
		auto it = mPushConstants.find(name);
		size_t sz = 0;
		if (it == mPushConstants.end()) {
			for (const auto& m : mModules)
				if (auto it = m->push_constants().find(name); it != m->push_constants().end()) {
					sz = it->second.mTypeSize;
					break;
			}
			if (sz == 0) throw invalid_argument("No push constant named " + name);
		} else
			sz = it->second.size();

		auto& c = mPushConstants[name];
		if constexpr (ranges::range<T>) {
			if (sizeof(ranges::range_value_t<T>)*ranges::size(v) != sz) throw invalid_argument("Argument must match push constant size");
			c.resize(ranges::range_value_t<T>*ranges::size(v));
			ranges::copy(v, span<ranges::range_value_t<T>>(c.data(), ranges::size(v)));
		} else {
			if (sizeof(T) != sz) throw invalid_argument("Argument must match push constant size");
			c.resize(sizeof(T));
			*reinterpret_cast<T*>(c.data()) = v;
		}
	}
	template<typename T = vector<byte>>
	inline const T& push_constant(const string& name) const {
		auto it = mPushConstants.find(name);
		if (it == mPushConstants.end()) throw invalid_argument("No push constant named " + name);
		if constexpr (is_same_v<T,vector<byte>>)
			return it->second;
		else {
			if (it->second.size() != sizeof(T)) throw invalid_argument("Type size must match push constant size");
			return *reinterpret_cast<T*>(it->second.data());
		}
	}

	inline uint32_t descriptor_count(const string& name) const {
		for (const auto& spirv : mModules)
			if (auto it = spirv->descriptors().find(name); it != spirv->descriptors().end()) {
				uint32_t count = 1;
				for (const auto& v : it->second.mArraySize)
					if (v.index() == 0)
						// array size is a literal
						count *= get<uint32_t>(v);
					else
						// array size is a specialization constant
						count *= specialization_constant(get<string>(v));
				return count;
			}
		return 0;
	}

	inline stm::Descriptor& descriptor(const string& name, uint32_t arrayIndex = 0) {		
		auto desc_it = mDescriptors.find(name);
		if (desc_it != mDescriptors.end()) {
			auto it = desc_it->second.find(arrayIndex);
			if (it != desc_it->second.end())
				return it->second;
		}

		for (const auto& spirv : mModules)
			if (spirv->descriptors().find(name) != spirv->descriptors().end()) {
				auto desc_it = mDescriptors.emplace(name, unordered_map<uint32_t, stm::Descriptor>()).first;
				auto it = desc_it->second.emplace(arrayIndex, stm::Descriptor()).first;
				return it->second;
			}
		
		throw invalid_argument("Descriptor " + name + " does not exist");
	}
	inline const stm::Descriptor& descriptor(const string& name, uint32_t arrayIndex = 0) const {
		return mDescriptors.at(name).at(arrayIndex);
	}

	inline void transition_images(CommandBuffer& commandBuffer) const {
		for (auto& [name, p] : mDescriptors)
			for (auto& [arrayIndex, d] : p)
				if (d.index() == 0)
					if (Texture::View view = get<Texture::View>(d))
						view.texture().transition_barrier(commandBuffer, get<vk::ImageLayout>(d));
	}

	STRATUM_API virtual shared_ptr<GraphicsPipeline> bind(CommandBuffer& commandBuffer, const Geometry& g);

	STRATUM_API void bind_descriptor_sets(CommandBuffer& commandBuffer, const unordered_map<string,uint32_t>& dynamicOffsets = {}) const;
	inline void push_constants(CommandBuffer& commandBuffer) const {
		for (const auto&[name, data] : mPushConstants)
			commandBuffer.push_constant(name, data);
	}
};

}