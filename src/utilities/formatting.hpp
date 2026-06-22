#ifndef PICO_FORMATTING_HH
#define PICO_FORMATTING_HH

#define REGISTER_FORMAT_OVERRIDE(type, ...) template <> \
struct std::formatter<type> : std::formatter<std::string_view> { \
    template <typename FormatContext> \
    auto format(const type& self, FormatContext& ctx) const { \
        std::string buffer = std::format(__VA_ARGS__); \
        return std::formatter<std::string_view>::format(buffer, ctx); \
    } \
};

#endif