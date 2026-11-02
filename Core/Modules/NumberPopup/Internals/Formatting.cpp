#include "Modules/NumberPopup/Internals/State.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {
std::string Fixed(double value, int decimals) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
}

bool IsZero(std::string_view value) { return value.find_first_not_of("-0.") == std::string_view::npos; }
} // namespace

void NumberPopup::State::ValidateValue(const NumberPopupValue& value) {
    if (const auto* decimal = std::get_if<double>(&value); decimal && !std::isfinite(*decimal))
        throw std::invalid_argument("Number popup value must be finite");
}

NumberPopupValue NumberPopup::State::Sum(const NumberPopupValue& left, const NumberPopupValue& right) {
    ValidateValue(right);
    if (const auto* a = std::get_if<std::int64_t>(&left)) {
        if (const auto* b = std::get_if<std::int64_t>(&right)) {
            if ((*b > 0 && *a > std::numeric_limits<std::int64_t>::max() - *b) ||
                (*b < 0 && *a < std::numeric_limits<std::int64_t>::min() - *b))
                throw std::overflow_error("Number popup integer sum overflow");
            return *a + *b;
        }
    }
    const auto decimal = [](const NumberPopupValue& value) {
        return std::visit([](auto number) { return static_cast<double>(number); }, value);
    };
    const double result = decimal(left) + decimal(right);
    if (!std::isfinite(result)) throw std::overflow_error("Number popup decimal sum overflow");
    return result;
}

void NumberPopup::State::ValidateFormat(const NumberPopupFormat& format) {
    if (format.decimals < 0 || format.decimals > 9 ||
        (format.sign != NumberPopupSign::NegativeOnly && format.sign != NumberPopupSign::Always &&
         format.sign != NumberPopupSign::None))
        throw std::invalid_argument("Invalid number popup format");
}

std::string NumberPopup::State::FormatValue(const NumberPopupValue& value, const NumberPopupFormat& format) {
    ValidateValue(value);
    ValidateFormat(format);
    if (format.formatter) return format.formatter(value);

    const double decimal = std::visit([](auto number) { return static_cast<double>(number); }, value);
    const bool abbreviated = format.compact && std::abs(decimal) >= 999.0;
    std::string body;
    std::string_view unit;
    if (const auto* integer = std::get_if<std::int64_t>(&value); integer && !abbreviated) {
        body = std::to_string(*integer);
        if (format.decimals > 0) body += "." + std::string(static_cast<std::size_t>(format.decimals), '0');
    } else {
        static constexpr std::array<std::string_view, 5> Units{"", "K", "M", "B", "T"};
        double scaled = decimal;
        std::size_t index = 0;
        if (format.compact) {
            while (std::abs(scaled) >= 1000.0 && index + 1 < Units.size()) {
                scaled /= 1000.0;
                ++index;
            }
            const double precision = std::pow(10.0, format.decimals);
            if (index + 1 < Units.size() && std::round(std::abs(scaled) * precision) / precision >= 1000.0) {
                scaled /= 1000.0;
                ++index;
            }
        }
        body = Fixed(scaled, format.decimals);
        unit = Units[index];
    }
    if (format.trimTrailingZeros && format.decimals > 0) {
        while (body.back() == '0')
            body.pop_back();
        if (body.back() == '.') body.pop_back();
    }
    if ((format.sign == NumberPopupSign::None || IsZero(body)) && body.front() == '-') body.erase(0, 1);
    if (format.sign == NumberPopupSign::Always && body.front() != '-') body.insert(body.begin(), '+');
    return format.prefix + body + std::string(unit) + format.suffix;
}
