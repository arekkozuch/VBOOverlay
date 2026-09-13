#pragma once

#include <functional>
#include <limits>
#include <stdexcept>
#include <string>

namespace FlappedEar {

using CancellationCheck = std::function<bool()>;

class OperationCancelled final : public std::runtime_error {
public:
    OperationCancelled() : std::runtime_error("Source operation cancelled.") {}
    explicit OperationCancelled(const std::string &message) : std::runtime_error(message) {}
};

class ResourceLimitError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void throwIfCancelled(const CancellationCheck &cancelled)
{
    if (cancelled && cancelled()) {
        throw OperationCancelled();
    }
}

} // namespace FlappedEar
