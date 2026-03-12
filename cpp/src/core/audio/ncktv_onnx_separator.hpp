/**
 * @file ncktv_onnx_separator.hpp
 * @brief Native ONNX Runtime C++ inference for UVR / MDX-Net Models
 *
 * Only available when NCKTV_HAS_ONNX=1 (MSVC builds with prebuilt ONNX Runtime).
 * On MinGW, vocal separation is handled via the Python subprocess bridge.
 */

#pragma once

#if NCKTV_HAS_ONNX

#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <future>
#include <array>
#include <iostream>

namespace ncktv {
namespace ai {

class VocalSeparator {
private:
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "NC-KTV-UVR"};
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};

    int channels_{2};
    int sample_rate_{44100};
    int segment_size_{256};
    
    std::vector<int64_t> input_shape_;
    std::vector<int64_t> output_shape_;

public:
    explicit VocalSeparator(const std::string& onnx_model_path) {
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(4);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        try {
            OrtCUDAProviderOptions cuda_options;
            session_options.AppendExecutionProvider_CUDA(cuda_options);
            std::cout << "UVR: CUDA Provider enabled successfully." << std::endl;
        } catch (const Ort::Exception& e) {
            std::cerr << "UVR: CUDA not available, falling back to CPU. (" << e.what() << ")" << std::endl;
        }

        #ifdef _WIN32
        std::wstring w_model_path(onnx_model_path.begin(), onnx_model_path.end());
        session_ = std::make_unique<Ort::Session>(env_, w_model_path.c_str(), session_options);
        #else
        session_ = std::make_unique<Ort::Session>(env_, onnx_model_path.c_str(), session_options);
        #endif

        Ort::AllocatorWithDefaultOptions allocator;
        auto input_type_info = session_->GetInputTypeInfo(0);
        auto tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        input_shape_ = tensor_info.GetShape();
        if (input_shape_[0] == -1) input_shape_[0] = 1;
    }

    size_t getInputExpectedSize() const {
        size_t size = 1;
        for (auto dim : input_shape_) size *= dim;
        return size;
    }

    std::future<std::pair<std::vector<float>, std::vector<float>>> separate_async(
        const std::vector<float>& mix_spectrogram
    ) {
        return std::async(std::launch::async, [this, mix_spectrogram]() {
            size_t input_tensor_size = 1;
            for (auto dim : input_shape_) input_tensor_size *= dim;

            if (mix_spectrogram.size() != input_tensor_size) {
                 throw std::runtime_error("Spectrogram size mismatch with model inputs");
            }

            std::vector<const char*> input_names = {"input"};
            std::vector<const char*> output_names = {"output"};

            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                memory_info_,
                const_cast<float*>(mix_spectrogram.data()),
                mix_spectrogram.size(),
                input_shape_.data(),
                input_shape_.size()
            );

            auto output_tensors = session_->Run(
                Ort::RunOptions{nullptr},
                input_names.data(),
                &input_tensor,
                1,
                output_names.data(),
                1
            );

            float* output_data = output_tensors[0].GetTensorMutableData<float>();
            auto out_info = output_tensors[0].GetTensorTypeAndShapeInfo();
            auto out_shape = out_info.GetShape();
            
            size_t total_elements = 1;
            for (auto dim : out_shape) total_elements *= dim;

            size_t half = total_elements / 2;
            std::vector<float> vocals(output_data, output_data + half);
            std::vector<float> instrumentals(output_data + half, output_data + total_elements);

            return std::make_pair(vocals, instrumentals);
        });
    }
};

} // namespace ai
} // namespace ncktv

#endif // NCKTV_HAS_ONNX
