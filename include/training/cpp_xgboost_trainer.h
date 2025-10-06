#pragma once

#include "training/feature_normalizer.h"
#include "strategy/xgb_feature_set.h"
#include "common/types.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

typedef void* DMatrixHandle;
typedef void* BoosterHandle;

namespace sentio {
namespace training {

// Forward declaration
class TrainingEvaluator;

class CppXGBoostTrainer {
public:
    struct TrainingConfig {
        std::string data_path;
        std::string output_dir;
        double train_ratio = 0.7;
        
        // NEW: Multi-bar prediction horizon
        int prediction_horizon = 5;  // Default to 5-bar prediction
        
        // XGBoost hyperparameters for REGRESSION
        int n_estimators = 500;
        double learning_rate = 0.02;
        int max_depth = 4;
        double subsample = 0.7;
        double colsample_bytree = 0.7;
        double reg_alpha = 1.0;
        double reg_lambda = 2.0;
        double min_child_weight = 5.0;
        double gamma = 0.1;
        
        // Training control
        bool enable_early_stopping = true;
        int early_stopping_rounds = 50;
        bool verbose = true;
    };

    struct TrainingResults {
        double final_rmse = 0.0;          
        double final_mae = 0.0;           
        double directional_accuracy = 0.0; 
        int prediction_horizon = 1;        // NEW: What horizon was trained
        std::string model_path;
        std::string config_path;
        std::string feature_names_path;
        std::map<std::string, double> feature_importance;
        double training_time_seconds = 0.0;
        size_t total_samples = 0;
        size_t total_features = 0;
        
        // Distribution metrics
        double positive_ratio = 0.0;  
        double zero_ratio = 0.0;      
        double negative_ratio = 0.0;  
        double mean_return = 0.0;     
        double std_return = 0.0;
        
        // NEW: Enhanced metrics storage
        std::map<std::string, std::string> metadata;
        double quality_score = 0.0;  // Overall quality score (0-100)
    };

    struct TrainingDataset {
        std::vector<std::vector<double>> features;
        std::vector<float> labels;
        std::vector<uint64_t> timestamps;
        std::vector<size_t> bar_indices;
    };

    explicit CppXGBoostTrainer(std::unique_ptr<XGBFeatureSet> feature_set);
    ~CppXGBoostTrainer();

    bool initialize(const TrainingConfig& config);
    TrainingResults train();
    bool export_model(const std::string& path);

    const TrainingConfig& get_config() const { return config_; }
    bool is_initialized() const { return initialized_; }
    size_t get_market_data_size() const { return market_data_.size(); }

private:
    // Modified for multi-bar prediction
    float create_return_label(size_t bar_index);
    
    TrainingDataset prepare_training_data();
    DMatrixHandle create_dmatrix(const TrainingDataset& data);
    BoosterHandle train_model(DMatrixHandle dtrain, DMatrixHandle dval);
    void set_xgboost_parameters(BoosterHandle booster);
    
    double evaluate_rmse(BoosterHandle booster, DMatrixHandle dtest);
    double evaluate_mae(BoosterHandle booster, DMatrixHandle dtest);
    double evaluate_directional_accuracy(BoosterHandle booster, DMatrixHandle dtest);
    
    std::map<std::string, double> extract_feature_importance(BoosterHandle booster);
    bool export_training_config(const std::string& path);
    bool export_feature_names(const std::string& path);
    
    std::unique_ptr<XGBFeatureSet> feature_set_;
    std::vector<Bar> market_data_;
    TrainingConfig config_;
    std::unique_ptr<FeatureNormalizer> feature_normalizer_;
    BoosterHandle booster_;
    bool initialized_ = false;
};

} // namespace training
} // namespace sentio