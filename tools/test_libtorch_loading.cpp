// test_libtorch_loading.cpp
#include <torch/torch.h>
#include <torch/script.h>
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::cout << "🔍 LibTorch Model Loading Test" << std::endl;
    std::cout << "📦 LibTorch Version: " << TORCH_VERSION << std::endl;
    
    std::string model_path = "artifacts/PPO/leveraged_trained/final_model.pt";
    if (argc > 1) {
        model_path = argv[1];
    }
    
    // Check file exists
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "❌ File not found: " << model_path << std::endl;
        return 1;
    }
    
    auto file_size = std::filesystem::file_size(model_path);
    std::cout << "✓ File exists (size: " << file_size << " bytes)" << std::endl;
    
    try {
        // Try loading with CPU
        std::cout << "\n📥 Loading model on CPU..." << std::endl;
        torch::jit::script::Module model;
        model = torch::jit::load(model_path, torch::kCPU);
        std::cout << "✓ Model loaded successfully" << std::endl;
        
        // Test with various input shapes
        std::vector<std::vector<int64_t>> test_shapes = {
            {1, 64, 16},
            {1, 16},
            {64, 16},
            {1, 32, 16}
        };
        
        std::cout << "\n🧪 Testing inference:" << std::endl;
        
        for (const auto& shape : test_shapes) {
            try {
                std::cout << "  Testing shape [";
                for (size_t i = 0; i < shape.size(); ++i) {
                    std::cout << shape[i];
                    if (i < shape.size() - 1) std::cout << ", ";
                }
                std::cout << "]... ";
                
                auto input = torch::randn(shape);
                std::vector<torch::jit::IValue> inputs;
                inputs.push_back(input);
                
                auto output = model.forward(inputs);
                
                if (output.isTensor()) {
                    auto tensor = output.toTensor();
                    std::cout << "✓ Output shape: " << tensor.sizes() << std::endl;
                    
                    // Found working configuration
                    std::cout << "\n✅ WORKING CONFIGURATION FOUND:" << std::endl;
                    std::cout << "   Input shape: [";
                    for (size_t i = 0; i < shape.size(); ++i) {
                        std::cout << shape[i];
                        if (i < shape.size() - 1) std::cout << ", ";
                    }
                    std::cout << "]" << std::endl;
                    std::cout << "   Output shape: " << tensor.sizes() << std::endl;
                    
                    return 0;
                } else if (output.isTuple()) {
                    std::cout << "✓ Output is tuple" << std::endl;
                    return 0;
                }
                
            } catch (const c10::Error& e) {
                std::cout << "✗ " << e.msg().substr(0, 50) << std::endl;
            }
        }
        
        std::cerr << "\n❌ No working input shape found" << std::endl;
        return 1;
        
    } catch (const c10::Error& e) {
        std::cerr << "\n❌ PyTorch Error: " << e.what() << std::endl;
        std::cerr << "   Full message: " << e.msg() << std::endl;
        
        // Check for common issues
        std::string error_msg = e.msg();
        if (error_msg.find("version") != std::string::npos) {
            std::cerr << "\n💡 Version Compatibility Issue Detected" << std::endl;
            std::cerr << "   The model may have been saved with a different PyTorch version" << std::endl;
            std::cerr << "   Current LibTorch: " << TORCH_VERSION << std::endl;
        }
        
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Standard Error: " << e.what() << std::endl;
        return 1;
    }
}
