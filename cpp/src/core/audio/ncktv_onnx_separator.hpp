/**
 * @file ncktv_onnx_separator.hpp
 * @brief Native ONNX Runtime C++ inference for UVR / MDX-Net Models
 *
 * Available on all Windows builds (MSVC and MinGW) when NCKTV_HAS_ONNX=1.
 * On MinGW, a MinGW-compatible import library (.a) is generated at build
 * time via dlltool from the prebuilt onnxruntime.dll.
 */

#pragma once

#if NCKTV_HAS_ONNX

// ── GCC/MinGW SAL compatibility shim ────────────────────────────────────────
// ONNX Runtime headers use MSVC SAL annotations extensively
// (_In_, _Out_, _Frees_ptr_opt_, etc.). GCC/MinGW has no SAL support, so we
// stub them all out to empty before the first ONNX include.
#if !defined(_MSC_VER) && !defined(__sal_h__)
#define __sal_h__          // prevent sal.h from being included at all
// Pointer annotations
#define _In_
#define _In_z_
#define _In_opt_
#define _In_opt_z_
#define _In_reads_(s)
#define _In_reads_opt_(s)
#define _In_reads_bytes_(s)
#define _In_reads_bytes_opt_(s)
#define _Out_
#define _Out_opt_
#define _Out_writes_(s)
#define _Out_writes_opt_(s)
#define _Out_writes_bytes_(s)
#define _Out_writes_bytes_opt_(s)
#define _Inout_
#define _Inout_opt_
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_maybenull_
#define _Outptr_opt_result_maybenull_
#define _Outptr_result_buffer_(s)
#define _Outptr_result_bytebuffer_(s)
#define _Outptr_result_z_
#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _Deref_out_z_
#define _Frees_ptr_
#define _Frees_ptr_opt_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Ret_z_
#define _Check_return_
#define _Must_inspect_result_
#define _Printf_format_string_
#define _Success_(e)
#define _When_(c, a)
#define _At_(t, a)
#define _Field_size_(s)
#define _Field_size_opt_(s)
#define _Field_size_bytes_(s)
#define _Field_size_bytes_opt_(s)
#define _Field_range_(lo, hi)
#define _Null_terminated_
#define _NullNull_terminated_
#define _Pre_notnull_
#define _Pre_maybenull_
#define _Post_z_
#define _Writable_elements_(s)
#define _Readable_elements_(s)
#define _Writable_bytes_(s)
#define _Readable_bytes_(s)
#define __drv_aliasesMem
#endif
// ─────────────────────────────────────────────────────────────────────────────

#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <future>
#include <array>
#include <memory>
#include <iostream>

namespace ncktv {
namespace ai {

class VocalSeparator {
public:  // exposed for per-segment inference in VocalSeparatorWorker
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "NC-KTV-UVR"};
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};

private:
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
            std::cout << "UVR: CUDA Provider requested." << std::endl;
        } catch (const Ort::Exception& e) {
            std::cerr << "UVR: CUDA fallback info: " << e.what() << std::endl;
        }

        try {
            #ifdef _WIN32
            std::wstring w_model_path(onnx_model_path.begin(), onnx_model_path.end());
            session_ = std::make_unique<Ort::Session>(env_, w_model_path.c_str(), session_options);
            #else
            session_ = std::make_unique<Ort::Session>(env_, onnx_model_path.c_str(), session_options);
            #endif

            if (!session_) throw std::runtime_error("Failed to create ONNX session");

            Ort::AllocatorWithDefaultOptions allocator;
            auto input_type_info = session_->GetInputTypeInfo(0);
            auto tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
            input_shape_ = tensor_info.GetShape();
            if (input_shape_[0] == -1) input_shape_[0] = 1;
            std::cout << "UVR: Session created successfully for " << onnx_model_path << std::endl;
        } catch (const Ort::Exception& e) {
             std::cerr << "UVR ERROR: Failed to create ONNX session: " << e.what() << std::endl;
             throw; // rethrow to worker
        } catch (const std::exception& e) {
             std::cerr << "UVR ERROR: " << e.what() << std::endl;
             throw;
        }
    }

    size_t getInputExpectedSize() const {
        if (!session_) return 0;
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
