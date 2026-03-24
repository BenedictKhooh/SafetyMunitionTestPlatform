#ifndef CONTACTCARD_H
#define CONTACTCARD_H
#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct ContactCard : public KeywordCard {
    std::string keyword;
    int ssid, msid;
    double fs, fd;

    ContactCard() = default;

    // 🌟 强类型参数构造
    ContactCard(const std::string& type, int slaveId, int masterId, double static_f, double dynamic_f) {
        keyword = type;
        ssid = slaveId;
        msid = masterId;
        fs = static_f;
        fd = dynamic_f;
    }

    // 🌟 核心多态方法实现
    std::string to_string() const override {
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "*CONTACT_%s\n"
            "$#    ssid      msid     sstyp     mstyp    sboxid    mboxid       spr       mpr\n"
            "%10d%10d         3         3         0         0         0         0\n"
            "$#      fs        fd        dc        vc       vdc    penchk        bt        dt\n"
            "%10.4f%10.4f       0.0       0.0       0.0         0       0.0       0.0\n"
            "$#     sfs       sfm       sst       mst      sfst      sfmt       fsf       vsf\n"
            "       1.0       1.0       0.0       0.0       0.0       0.0       1.0       1.0\n",
            keyword.c_str(), ssid, msid, fs, fd
        );

        if (keyword.find("ERODING") != std::string::npos) {
            char eroBuf[256];
            snprintf(eroBuf, sizeof(eroBuf),
                "$#    isim    erodes\n"
                "         0         0\n"
            );
            return std::string(buf) + std::string(eroBuf);
        }
        return std::string(buf);
    }
};
#endif