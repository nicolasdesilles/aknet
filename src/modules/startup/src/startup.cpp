//
// Created by Nicolas Désilles on 06/01/2026.
//

#include "startup.h"

namespace aknet::startup::types {

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    StepProgress make_initial_step_progress(const StepConfig &config) {

        return StepProgress{
        .id = config.id,
        .display_name = config.display_name,
        .status = StepStatus::Pending};

    }

    SequenceProgress make_initial_sequence_progress(AppState state, const std::vector<StepConfig>& configs) {

        std::vector<StepProgress> steps;
        steps.reserve(configs.size());

        for (const auto &config : configs) {
            steps.push_back(make_initial_step_progress(config));
        }

        return SequenceProgress{
        .state = state,
        .steps = steps};

    }

    Result validate_step_config(const StepConfig &config) {

        if (empty(config.id)) {
            return {.ok = false, .error = "Step id cannot be empty"};
        }

        if (empty(config.display_name)) {
            return {.ok = false, .error = "Step display name cannot be empty"};
        }

        if (config.timeout.count() < 0) {
            return {.ok = false, .error = "Step timeout must be equal or greater than 0"};
        }

        return {.ok = true};
    }

    Result validate_step_configs_unique(const std::vector<StepConfig> &configs) {

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
