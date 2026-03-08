#include "MaterialCard.h"
#include "src/core/common/DynaFormat.h"
#include <sstream>

// Johnson-Cook 实现
MaterialJohnsonCook::MaterialJohnsonCook(int id, std::string t) : mid(id), title(t) {
    ro = 0; g = 0; e = 0; pr = 0; a = 0; b = 0; n = 0; c = 0; m = 0; tm = 0; tr = 0; epso = 0;
    cp = 0; pc = 0; spall = 0; it = 0; d1 = 0; d2 = 0; d3 = 0; d4 = 0; d5 = 0;
}
std::string MaterialJohnsonCook::generate() const {
    std::stringstream ss;
    ss << "*MAT_JOHNSON_COOK_TITLE\n" << title << "\n";
    ss << "$#     mid        ro         g         e        pr       dtf        vp    rateop\n";
    ss << DynaFormat::I10(mid) << DynaFormat::F10(ro) << DynaFormat::F10(g) << DynaFormat::F10(e) << DynaFormat::F10(pr) << "       0.0       0.0       0.0\n";
    ss << "$#       a         b         n         c         m        tm        tr      epso\n";
    ss << DynaFormat::F10(a) << DynaFormat::F10(b) << DynaFormat::F10(n) << DynaFormat::F10(c) << DynaFormat::F10(m) << DynaFormat::F10(tm) << DynaFormat::F10(tr) << DynaFormat::F10(epso) << "\n";
    ss << "$#      cp        pc     spall        it        d1        d2        d3        d4\n";
    ss << DynaFormat::E10(cp) << DynaFormat::F10(pc) << DynaFormat::F10(spall) << DynaFormat::F10(it) << DynaFormat::F10(d1) << DynaFormat::F10(d2) << DynaFormat::F10(d3) << DynaFormat::F10(d4) << "\n";
    ss << "$#      d5      c2/p      erod     efmin    numint\n";
    ss << DynaFormat::F10(d5) << "       0.0" << "       0.0" << "1.00000E-6" << "       0.0\n";
    return ss.str();
}

// Lee-Tarver 实现
EOSLeeTarver::EOSLeeTarver(int id) : eosid(id) {}
std::string EOSLeeTarver::generate() const {
    std::stringstream ss;
    ss << "*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE_TITLE\nLeeTarver\n";
    ss << "$#   eosid         a         b       xp1       xp2      frer         g        r1\n";
    ss << DynaFormat::I10(eosid) << DynaFormat::F10(a) << DynaFormat::F10(b) << DynaFormat::F10(xp1) << DynaFormat::F10(xp2) << DynaFormat::F10(frer) << DynaFormat::E10(g) << DynaFormat::F10(r1) << "\n";
    ss << "$#      r2        r3        r5        r6     fmxig      freq     grow1        em\n";
    ss << DynaFormat::F10(r2) << DynaFormat::E10(r3) << DynaFormat::F10(r5) << DynaFormat::F10(r6) << DynaFormat::F10(fmxig) << DynaFormat::F10(freq) << DynaFormat::F10(grow1) << DynaFormat::F10(em) << "\n";
    ss << "$#     ar1       es1       cvp       cvr     eetal     ccrit       enq      tmp0\n";
    ss << DynaFormat::F10(ar1) << DynaFormat::F10(es1) << DynaFormat::F10(cvp) << DynaFormat::F10(cvr) << DynaFormat::F10(eetal) << DynaFormat::F10(ccrit) << DynaFormat::F10(enq) << DynaFormat::F10(tmp0) << "\n";
    ss << "$#   grow2       ar2       es2        en     fmxgr     fmngr\n";
    ss << DynaFormat::F10(grow2) << DynaFormat::F10(ar2) << DynaFormat::F10(es2) << DynaFormat::F10(en) << DynaFormat::F10(fmxgr) << DynaFormat::F10(0.0) << "\n";
    return ss.str();
}

// Gruneisen 实现
EOSGruneisen::EOSGruneisen(int id, double C_val, double S1_val, double G_val, double A_val) : eosid(id), c(C_val), s1(S1_val), gamao(G_val), a(A_val), s2(0), s3(0), e0(0), v0(1.0) {}
std::string EOSGruneisen::generate() const {
    std::stringstream ss;
    ss << "*EOS_GRUNEISEN_TITLE\nGruneisen_EOS\n";
    ss << "$#   eosid         c        s1        s2        s3     gamao         a        e0\n";
    ss << DynaFormat::I10(eosid) << DynaFormat::F10(c) << DynaFormat::F10(s1) << DynaFormat::F10(s2) << DynaFormat::F10(s3) << DynaFormat::F10(gamao) << DynaFormat::F10(a) << DynaFormat::F10(e0) << "\n";
    ss << "$#      v0\n" << DynaFormat::F10(v0) << "\n";
    return ss.str();
}

// MatAddErosion 实现
MatAddErosion::MatAddErosion(int m_id, double effective_strain, double max_strain) : mid(m_id), effeps(effective_strain), mxeps(max_strain) {}
std::string MatAddErosion::generate() const {
    std::stringstream ss;
    ss << "*MAT_ADD_EROSION\n";
    ss << "$#     mid      excl    mxpres     mneps    effeps    voleps    numfip       ncs\n";
    ss << DynaFormat::I10(mid) << "       0.0       0.0       0.0" << DynaFormat::F10(effeps) << "       0.0       1.0       1.0\n";
    ss << "$#  mnpres     sigp1     sigvm     mxeps     epssh     sigth   impulse    failtm\n";
    ss << "       0.0       0.0       0.0" << DynaFormat::F10(mxeps) << "       0.0       0.0       0.0       0.0\n";
    return ss.str();
}

// Plastic Kinematic 实现
MatPlasticKinematic::MatPlasticKinematic(int id, double r, double elastic, double p_ratio, double yield, double tangent)
    : mid(id), ro(r), e(elastic), pr(p_ratio), sigy(yield), etan(tangent), beta(1.0) {
}
std::string MatPlasticKinematic::generate() const {
    std::stringstream ss;
    ss << "*MAT_PLASTIC_KINEMATIC_TITLE\nshell_kinematic\n";
    ss << "$#     mid        ro         e        pr      sigy      etan      beta\n";
    ss << DynaFormat::I10(mid) << DynaFormat::F10(ro) << DynaFormat::F10(e) << DynaFormat::F10(pr) << DynaFormat::F10(sigy) << DynaFormat::F10(etan) << DynaFormat::F10(beta) << "\n";
    ss << "$#     src       srp        fs        vp\n       0.0       0.0       1.0       1.0\n";
    return ss.str();
}