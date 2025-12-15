#pragma once

#include <optional>
#include <cstdint>
#include <string>

namespace Masstree {

class LearnedIndex {
public:
    virtual ~LearnedIndex() {}

    // Predict approximate key order value.
    virtual std::optional<uint64_t> predict(const std::string& key) = 0;

    // Optional incremental training
    virtual void train(const std::string& key) {}

    // Model readiness
    virtual bool ready() const { return false; }
};

} // namespace Masstree 