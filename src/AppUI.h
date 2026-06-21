#pragma once
#include "ui/LocoUI.h"
#include "ui/AccessoriesUI.h"
#include "ui/PowerUI.h"
#include <memory>

// Owning pointers live in main.cpp — imported here for demo mode access
extern std::unique_ptr<LocoUI>        locoUI;
extern std::unique_ptr<AccessoriesUI> accUI;
extern std::unique_ptr<PowerUI>       pwrUI;
