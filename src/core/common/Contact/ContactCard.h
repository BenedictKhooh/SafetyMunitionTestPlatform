#ifndef CONTACTCARD_H
#define CONTACTCARD_H
#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct ContactCard : public KeywordCard {
    std::string keyword;
    int ssid = 1, msid = 1;
    double fs, fd;

    ContactCard() = default;

    ContactCard(const std::string& type, int slaveId, int masterId, double static_f, double dynamic_f) {
        keyword = type;
        ssid = slaveId;
        msid = masterId;
        fs = static_f;
        fd = dynamic_f;
    }

    std::string to_string() const override {
        char buf[2048];

        // ==========================================
        // 类型 1: 侵蚀面到面接触 (需要最完整、最复杂的 6 行参数)
        // ==========================================
        if (keyword.find("ERODING") != std::string::npos) {
            snprintf(buf, sizeof(buf),
                "*CONTACT_%s\n"
                "$#     cid                                                                         title\n"
                "$1    SSID      MSID     SSTYP     MSTYP    SBOXID    MBOXID       SPR       MPR\n"
                "$#    ssid      msid     sstyp     mstyp    sboxid    mboxid       spr       mpr\n"
                "%10d%10d         3         3         0         0         1         1\n"
                "$2      FS        FD        DC        VC       VDC    PENCHK        BT        DT\n"
                "$#      fs        fd        dc        vc       vdc    penchk        bt        dt\n"
                "%10.4f%10.4f       0.0    0.0017      20.0         2       0.01.00000E20\n"
                "$3     SFS       SFM       SST       MST      SFST      SFMT       FSF       VSF\n"
                "$#     sfs       sfm       sst       mst      sfst      sfmt       fsf       vsf\n"
                "       1.0       1.0       0.0       0.0       1.0       1.0       1.0       1.0\n"
                "$4    isym    erosop      iadj\n"
                "$#    isym    erosop      iadj\n"
                "         0         1         1\n"
                "$A    SFOT\n"
                "$#    soft    sofscl    lcidab    maxpar     sbopt     depth     bsort    frcfrq\n"
                "         2       0.1         0     1.025       2.0         1         0         1\n"
                "$B\n"
                "$#  penmax    thkopt    shlthk     snlog      isym     i2d3d    sldthk    sldstf\n"
                "       0.0         0         0         0         0         0       0.0       0.0\n"
                "$C\n"
                "$#    igap    ignore    dprfac    dtstif   unused     unused    flangl   cid_rcf\n"
                "                                     0.0         0\n",
                keyword.c_str(), ssid, msid, fs, fd);
        }
        // ==========================================
        // 类型 2: 自动单面接触 (只有从面，强制把 MSID 和 MSTYP 置为 0)
        // ==========================================
        else if (keyword.find("AUTOMATIC_SINGLE_SURFACE") != std::string::npos) {
            snprintf(buf, sizeof(buf),
                "*CONTACT_%s\n"
                "$#     cid                                                                         title\n"
                "$1    SSID      MSID     SSTYP     MSTYP    SBOXID    MBOXID       SPR       MPR\n"
                "$#    ssid      msid     sstyp     mstyp    sboxid    mboxid       spr       mpr\n"
                "%10d         0         3         0         0         0         0         0\n" // 🌟 MSID 强制为 0，MSTYP 强制为 0
                "$2      FS        FD        DC        VC       VDC    PENCHK        BT        DT\n"
                "$#      fs        fd        dc        vc       vdc    penchk        bt        dt\n"
                "%10.4f%10.4f       0.0       0.0       0.0         0       0.0       0.0\n"
                "$3     SFS       SFM       SST       MST      SFST      SFMT       FSF       VSF\n"
                "$#     sfs       sfm       sst       mst      sfst      sfmt       fsf       vsf\n"
                "       1.0       1.0       0.0       0.0       1.0       1.0       1.0       1.0\n",
                keyword.c_str(), ssid, fs, fd); // 🌟 这里只传了 ssid，省去了 msid 的传递
        }
        // ==========================================
        // 类型 3: 固连接触或其他常规接触 (主从面都需要，常规 3 行参数)
        // ==========================================
        else {
            snprintf(buf, sizeof(buf),
                "*CONTACT_%s\n"
                "$#     cid                                                                         title\n"
                "$1    SSID      MSID     SSTYP     MSTYP    SBOXID    MBOXID       SPR       MPR\n"
                "$#    ssid      msid     sstyp     mstyp    sboxid    mboxid       spr       mpr\n"
                "%10d%10d         3         3         0         0         1         1\n"
                "$2      FS        FD        DC        VC       VDC    PENCHK        BT        DT\n"
                "$#      fs        fd        dc        vc       vdc    penchk        bt        dt\n"
                "%10.4f%10.4f       0.0       0.0       0.0         0       0.0       0.0\n"
                "$3     SFS       SFM       SST       MST      SFST      SFMT       FSF       VSF\n"
                "$#     sfs       sfm       sst       mst      sfst      sfmt       fsf       vsf\n"
                "       1.0       1.0       0.0       0.0       1.0       1.0       1.0       1.0\n",
                keyword.c_str(), ssid, msid, fs, fd);
        }

        return std::string(buf);
    }

    // 兼容可能遗留的旧代码调用
    virtual std::string generate() const {
        return to_string();
    }
};
#endif