#include "core/data_manager.h"
#include "common/utils.h"

namespace sentio {

void DataManager::load_market_data(const std::string& path) {
    bars_ = utils::read_csv_data(path); // read_csv_data already populates bar_id and derived fields
    id_to_index_.clear();
    id_to_index_.reserve(bars_.size());
    for (size_t i = 0; i < bars_.size(); ++i) {
        id_to_index_[bars_[i].bar_id] = i;
    }
}

const Bar* DataManager::get_bar(uint64_t bar_id) const {
    auto it = id_to_index_.find(bar_id);
    if (it == id_to_index_.end()) return nullptr;
    size_t idx = it->second;
    if (idx >= bars_.size()) return nullptr;
    return &bars_[idx];
}

const Bar* DataManager::get_bar_by_index(size_t index) const {
    if (index >= bars_.size()) return nullptr;
    return &bars_[index];
}

} // namespace sentio



