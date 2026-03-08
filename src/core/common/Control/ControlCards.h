#pragma once
#include "src/core/common/KeywordCard.h"

// *CONTROL_BULK_VISCOSITY
class BulkViscosityCard : public KeywordCard {
public:
    std::string generate() const override;
};

// *CONTROL_CONTACT (参数较多，这里封装关键行)
class ControlContactCard : public KeywordCard {
public:
    std::string generate() const override;
};

// *CONTROL_ENERGY
class EnergyCard : public KeywordCard {
public:
    std::string generate() const override;
};

// *CONTROL_TERMINATION
class TerminationCard : public KeywordCard {
private:
    double endtim;
public:
    TerminationCard(double t) : endtim(t) {}
    std::string generate() const override;
};

// *CONTROL_TIMESTEP
class TimestepCard : public KeywordCard {
public:
    std::string generate() const override;
};