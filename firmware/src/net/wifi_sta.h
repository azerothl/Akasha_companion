#pragma once

#include "config/nvs_config.h"

bool wifi_connect(const CompanionConfig &cfg, uint32_t timeout_ms = 20000);
