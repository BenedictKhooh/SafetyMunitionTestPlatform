#pragma once
#include <string>
#include <cstdio>

// 格式化工具 (解决 LS-DYNA 严格的 10 字符对齐问题)
class DynaFormat {
public:
    static std::string I10(int val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%10d", val);
        return std::string(buf);
    }

    static std::string F10(double val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%10.3f", val);
        return std::string(buf);
    }

    static std::string E10(double val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%10.3E", val);
        return std::string(buf);
    }
};
