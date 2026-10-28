#include "ui/MaterialFormat.h"

namespace lholo::ui {

std::string_view materialIconId(std::string const& itemId, std::string const& typeName) {
    return itemId.empty() ? std::string_view{typeName} : std::string_view{itemId};
}
std::string formatMaterialHudCount(std::uint64_t count, int stackSize, bool convertStacks) {
    auto const stack = static_cast<std::uint64_t>(stackSize > 0 ? stackSize : 64);
    if (!convertStacks || count < stack) return std::to_string(count);
    auto result = std::to_string(count / stack) + "x" + std::to_string(stack);
    if (auto remainder = count % stack; remainder != 0)
        result += "+" + std::to_string(remainder);
    return result;
}

char const* materialDisplayName(
    std::string const&                  displayName,
    std::optional<i18n::TextKey> const& nameKey
) {
    return nameKey ? i18n::tr(*nameKey) : displayName.c_str();
}

}
