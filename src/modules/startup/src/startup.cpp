//
// Created by Nicolas Désilles on 06/01/2026.
//

#include "startup.h"

namespace aknet::startup {

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    Result validate_step_config(const StepConfig &config) {

        if (empty(config.id)) {
            return {.ok = false, .error = "Step id cannot be empty"};
        }

        if (empty(config.display_name)) {
            return {.ok = false, .error = "Step display name cannot be empty"};
        }

        if (config.timeout.count() <= 0) {
            return {.ok = false, .error = "Step timeout must be greater than 0"};
        }

        return {.ok = true};
    }

    Result validate_step_configs_unique(std::vector<StepConfig> &configs) {

        for (size_t i = 0; i < configs.size(); ++i) {
            for (size_t j = i + 1; j < configs.size(); ++j) {
                if (configs[i].id == configs[j].id) {
                    return {.ok = false, .error = "Duplicate step id found: " + configs[i].id};
                }
            }
        }
        return {.ok = true};
    }
}
