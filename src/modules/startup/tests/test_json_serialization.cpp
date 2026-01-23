//
// Created by Nicolas Désilles on 23/01/2026.
//

#include "test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

// ------------------------------------------------------------------------------------------------
// JSON Serialization Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Startup | JSON - AppState serialization", "[startup][json]") {
    using namespace startup;

    SECTION("to_json converts AppState to integer") {
        nlohmann::json j;

        to_json(j, AppState::Off);
        REQUIRE(j.is_number_integer());
        REQUIRE(j.get<int>() == 0);

        to_json(j, AppState::Booting);
        REQUIRE(j.get<int>() == 1);

        to_json(j, AppState::Active);
        REQUIRE(j.get<int>() == 2);

        to_json(j, AppState::ShuttingDown);
        REQUIRE(j.get<int>() == 3);
    }

    SECTION("from_json converts integer to AppState") {
        AppState state;

        from_json(nlohmann::json(0), state);
        REQUIRE(state == AppState::Off);

        from_json(nlohmann::json(1), state);
        REQUIRE(state == AppState::Booting);

        from_json(nlohmann::json(2), state);
        REQUIRE(state == AppState::Active);

        from_json(nlohmann::json(3), state);
        REQUIRE(state == AppState::ShuttingDown);
    }

    SECTION("round-trip AppState") {
        for (auto original : {AppState::Off, AppState::Booting, AppState::Active, AppState::ShuttingDown}) {
            nlohmann::json j = original;
            AppState restored = j.get<AppState>();
            REQUIRE(restored == original);
        }
    }

    SECTION("from_json throws on invalid integer") {
        AppState state;
        REQUIRE_THROWS(from_json(nlohmann::json(999), state));
    }
}

TEST_CASE("Startup | JSON - StepStatus serialization", "[startup][json]") {
    using namespace startup;

    SECTION("to_json converts StepStatus to integer") {
        nlohmann::json j;

        to_json(j, StepStatus::Pending);
        REQUIRE(j.get<int>() == 0);

        to_json(j, StepStatus::Running);
        REQUIRE(j.get<int>() == 1);

        to_json(j, StepStatus::Success);
        REQUIRE(j.get<int>() == 2);

        to_json(j, StepStatus::Failed);
        REQUIRE(j.get<int>() == 3);

        to_json(j, StepStatus::TimedOut);
        REQUIRE(j.get<int>() == 4);

        to_json(j, StepStatus::Skipped);
        REQUIRE(j.get<int>() == 5);

        to_json(j, StepStatus::Aborted);
        REQUIRE(j.get<int>() == 6);
    }

    SECTION("from_json converts integer to StepStatus") {
        StepStatus status;

        from_json(nlohmann::json(0), status);
        REQUIRE(status == StepStatus::Pending);

        from_json(nlohmann::json(6), status);
        REQUIRE(status == StepStatus::Aborted);
    }

    SECTION("round-trip StepStatus") {
        for (auto original : {StepStatus::Pending, StepStatus::Running, StepStatus::Success,
                              StepStatus::Failed, StepStatus::TimedOut, StepStatus::Skipped, StepStatus::Aborted}) {
            nlohmann::json j = original;
            StepStatus restored = j.get<StepStatus>();
            REQUIRE(restored == original);
        }
    }

    SECTION("from_json throws on invalid integer") {
        StepStatus status;
        REQUIRE_THROWS(from_json(nlohmann::json(999), status));
    }
}

TEST_CASE("Startup | JSON - AbortReason serialization", "[startup][json]") {
    using namespace startup;

    SECTION("to_json converts AbortReason to integer") {
        nlohmann::json j;

        to_json(j, AbortReason::None);
        REQUIRE(j.get<int>() == 0);

        to_json(j, AbortReason::UserRequested);
        REQUIRE(j.get<int>() == 1);

        to_json(j, AbortReason::Timeout);
        REQUIRE(j.get<int>() == 2);

        to_json(j, AbortReason::CriticalFailure);
        REQUIRE(j.get<int>() == 3);

        to_json(j, AbortReason::SystemShutdown);
        REQUIRE(j.get<int>() == 4);
    }

    SECTION("round-trip AbortReason") {
        for (auto original : {AbortReason::None, AbortReason::UserRequested, AbortReason::Timeout,
                              AbortReason::CriticalFailure, AbortReason::SystemShutdown}) {
            nlohmann::json j = original;
            AbortReason restored = j.get<AbortReason>();
            REQUIRE(restored == original);
        }
    }

    SECTION("from_json throws on invalid integer") {
        AbortReason reason;
        REQUIRE_THROWS(from_json(nlohmann::json(-1), reason));
        REQUIRE_THROWS(from_json(nlohmann::json(999), reason));
    }
}

TEST_CASE("Startup | JSON - Result serialization", "[startup][json]") {
    using namespace startup;

    SECTION("to_json converts Result to object") {
        Result result{true, ""};
        nlohmann::json j = result;

        REQUIRE(j.is_object());
        REQUIRE(j["ok"].get<bool>() == true);
        REQUIRE(j["error"].get<std::string>().empty());
    }

    SECTION("to_json includes error message") {
        Result result{false, "Something went wrong"};
        nlohmann::json j = result;

        REQUIRE(j["ok"].get<bool>() == false);
        REQUIRE(j["error"].get<std::string>() == "Something went wrong");
    }

    SECTION("from_json converts object to Result") {
        nlohmann::json j = {{"ok", true}, {"error", ""}};
        Result result = j.get<Result>();

        REQUIRE(result.ok == true);
        REQUIRE(result.error.empty());
    }

    SECTION("round-trip Result") {
        Result original{false, "Test error"};
        nlohmann::json j = original;
        Result restored = j.get<Result>();

        REQUIRE(restored.ok == original.ok);
        REQUIRE(restored.error == original.error);
    }
}

TEST_CASE("Startup | JSON - StepConfig serialization", "[startup][json]") {
    using namespace startup;

    SECTION("to_json converts StepConfig to object") {
        StepConfig config{
            .id = "test_step",
            .display_name = "Test Step",
            .timeout = std::chrono::seconds{30},
            .critical = true,
            .can_skip = false
        };

        nlohmann::json j = config;

        REQUIRE(j["id"].get<std::string>() == "test_step");
        REQUIRE(j["display_name"].get<std::string>() == "Test Step");
        REQUIRE(j["timeout"].get<int>() == 30);  // Timeout in seconds
        REQUIRE(j["critical"].get<bool>() == true);
        REQUIRE(j["can_skip"].get<bool>() == false);
    }

    SECTION("from_json converts object to StepConfig") {
        nlohmann::json j = {
            {"id", "my_step"},
            {"display_name", "My Step"},
            {"timeout", 60},
            {"critical", false},
            {"can_skip", true}
        };

        StepConfig config = j.get<StepConfig>();

        REQUIRE(config.id == "my_step");
        REQUIRE(config.display_name == "My Step");
        REQUIRE(config.timeout == std::chrono::seconds{60});
        REQUIRE(config.critical == false);
        REQUIRE(config.can_skip == true);
    }

    SECTION("round-trip StepConfig") {
        StepConfig original{
            .id = "step_1",
            .display_name = "Step One",
            .timeout = std::chrono::seconds{45},
            .critical = true,
            .can_skip = false
        };

        nlohmann::json j = original;
        StepConfig restored = j.get<StepConfig>();

        REQUIRE(restored.id == original.id);
        REQUIRE(restored.display_name == original.display_name);
        REQUIRE(restored.timeout == original.timeout);
        REQUIRE(restored.critical == original.critical);
        REQUIRE(restored.can_skip == original.can_skip);
    }

    SECTION("timeout of zero serializes correctly") {
        StepConfig config{
            .id = "no_timeout",
            .display_name = "No Timeout",
            .timeout = std::chrono::seconds{0}
        };

        nlohmann::json j = config;
        REQUIRE(j["timeout"].get<int>() == 0);

        StepConfig restored = j.get<StepConfig>();
        REQUIRE(restored.timeout == std::chrono::seconds{0});
    }
}

TEST_CASE("Startup | JSON - StepProgress serialization", "[startup][json]") {
    using namespace startup;

    SECTION("to_json converts StepProgress with no times") {
        StepProgress progress{
            .id = "step1",
            .display_name = "Step 1",
            .status = StepStatus::Pending,
            .message = "Waiting",
            .start_time = std::nullopt,
            .end_time = std::nullopt
        };

        nlohmann::json j = progress;

        REQUIRE(j["id"].get<std::string>() == "step1");
        REQUIRE(j["display_name"].get<std::string>() == "Step 1");
        REQUIRE(j["status"].get<int>() == 0);  // Pending
        REQUIRE(j["message"].get<std::string>() == "Waiting");
        REQUIRE(j["start_time"].is_null());
        REQUIRE(j["end_time"].is_null());
    }

    SECTION("to_json converts StepProgress with start_time") {
        auto now = std::chrono::steady_clock::now();

        StepProgress progress{
            .id = "step2",
            .display_name = "Step 2",
            .status = StepStatus::Running,
            .message = "In progress",
            .start_time = now,
            .end_time = std::nullopt
        };

        nlohmann::json j = progress;

        REQUIRE(j["status"].get<int>() == 1);  // Running
        REQUIRE(j["start_time"].is_number());
        REQUIRE(j["end_time"].is_null());
    }

    SECTION("to_json converts StepProgress with both times") {
        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::milliseconds{1234};

        StepProgress progress{
            .id = "step3",
            .display_name = "Step 3",
            .status = StepStatus::Success,
            .message = "Completed",
            .start_time = start,
            .end_time = end
        };

        nlohmann::json j = progress;

        REQUIRE(j["status"].get<int>() == 2);  // Success
        REQUIRE(j["start_time"].is_number());
        REQUIRE(j["end_time"].is_number());

        // Verify end_time > start_time
        REQUIRE(j["end_time"].get<int64_t>() > j["start_time"].get<int64_t>());
    }

    SECTION("from_json converts object with null times") {
        nlohmann::json j = {
            {"id", "test"},
            {"display_name", "Test"},
            {"status", 0},
            {"message", "msg"},
            {"start_time", nullptr},
            {"end_time", nullptr}
        };

        StepProgress progress = j.get<StepProgress>();

        REQUIRE(progress.id == "test");
        REQUIRE(progress.status == StepStatus::Pending);
        REQUIRE(progress.start_time == std::nullopt);
        REQUIRE(progress.end_time == std::nullopt);
    }

    SECTION("from_json converts object with times") {
        nlohmann::json j = {
            {"id", "test"},
            {"display_name", "Test"},
            {"status", 2},
            {"message", "done"},
            {"start_time", 1000},
            {"end_time", 2500}
        };

        StepProgress progress = j.get<StepProgress>();

        REQUIRE(progress.start_time.has_value());
        REQUIRE(progress.end_time.has_value());
    }

    SECTION("round-trip StepProgress without times") {
        StepProgress original{
            .id = "step_x",
            .display_name = "Step X",
            .status = StepStatus::Failed,
            .message = "Error occurred"
        };

        nlohmann::json j = original;
        StepProgress restored = j.get<StepProgress>();

        REQUIRE(restored.id == original.id);
        REQUIRE(restored.display_name == original.display_name);
        REQUIRE(restored.status == original.status);
        REQUIRE(restored.message == original.message);
        REQUIRE(restored.start_time == std::nullopt);
        REQUIRE(restored.end_time == std::nullopt);
    }

    SECTION("empty message serializes correctly") {
        StepProgress progress{
            .id = "step",
            .display_name = "Step",
            .status = StepStatus::Pending,
            .message = ""
        };

        nlohmann::json j = progress;
        REQUIRE(j["message"].get<std::string>().empty());
    }
}

TEST_CASE("Startup | JSON - SequenceProgress serialization", "[startup][json]") {
    using namespace startup;

    SECTION("to_json converts empty SequenceProgress") {
        SequenceProgress progress{
            .state = AppState::Off,
            .current_step_index = -1,
            .steps = {},
            .last_error = std::nullopt,
            .can_retry = false,
            .abort_reason = AbortReason::None
        };

        nlohmann::json j = progress;

        REQUIRE(j["state"].get<int>() == 0);  // Off
        REQUIRE(j["current_step_index"].get<int>() == -1);
        REQUIRE(j["steps"].is_array());
        REQUIRE(empty(j["steps"]));
        REQUIRE(j["last_error"].is_null());
        REQUIRE(j["can_retry"].get<bool>() == false);
        REQUIRE(j["abort_reason"].get<int>() == 0);  // None
    }

    SECTION("to_json converts SequenceProgress with steps") {
        SequenceProgress progress;
        progress.state = AppState::Booting;
        progress.current_step_index = 1;
        progress.steps.push_back(StepProgress{
            .id = "step1",
            .display_name = "Step 1",
            .status = StepStatus::Success,
            .message = "Done"
        });
        progress.steps.push_back(StepProgress{
            .id = "step2",
            .display_name = "Step 2",
            .status = StepStatus::Running,
            .message = "In progress"
        });
        progress.last_error = std::nullopt;
        progress.can_retry = false;
        progress.abort_reason = AbortReason::None;

        nlohmann::json j = progress;

        REQUIRE(j["state"].get<int>() == 1);  // Booting
        REQUIRE(j["current_step_index"].get<int>() == 1);
        REQUIRE(j["steps"].is_array());
        REQUIRE(j["steps"].size() == 2);
        REQUIRE(j["steps"][0]["id"].get<std::string>() == "step1");
        REQUIRE(j["steps"][1]["id"].get<std::string>() == "step2");
        REQUIRE(j["last_error"].is_null());
    }

    SECTION("to_json includes last_error when present") {
        SequenceProgress progress;
        progress.state = AppState::Off;
        progress.last_error = "Critical failure in step 3";
        progress.can_retry = true;
        progress.abort_reason = AbortReason::CriticalFailure;

        nlohmann::json j = progress;

        REQUIRE(j["last_error"].get<std::string>() == "Critical failure in step 3");
        REQUIRE(j["can_retry"].get<bool>() == true);
        REQUIRE(j["abort_reason"].get<int>() == 3);  // CriticalFailure
    }

    SECTION("from_json converts object to SequenceProgress") {
        nlohmann::json j = {
            {"state", 2},  // Active
            {"current_step_index", 2},
            {"steps", nlohmann::json::array()},
            {"last_error", nullptr},
            {"can_retry", false},
            {"abort_reason", 0}
        };

        SequenceProgress progress = j.get<SequenceProgress>();

        REQUIRE(progress.state == AppState::Active);
        REQUIRE(progress.current_step_index == 2);
        REQUIRE(empty(progress.steps));
        REQUIRE(progress.last_error == std::nullopt);
        REQUIRE(progress.can_retry == false);
        REQUIRE(progress.abort_reason == AbortReason::None);
    }

    SECTION("from_json converts object with steps array") {
        nlohmann::json j = {
            {"state", 1},
            {"current_step_index", 0},
            {"steps", nlohmann::json::array({
                {
                    {"id", "s1"},
                    {"display_name", "Step 1"},
                    {"status", 2},
                    {"message", "OK"},
                    {"start_time", nullptr},
                    {"end_time", nullptr}
                }
            })},
            {"last_error", nullptr},
            {"can_retry", false},
            {"abort_reason", 0}
        };

        SequenceProgress progress = j.get<SequenceProgress>();

        REQUIRE(progress.steps.size() == 1);
        REQUIRE(progress.steps[0].id == "s1");
        REQUIRE(progress.steps[0].status == StepStatus::Success);
    }

    SECTION("round-trip SequenceProgress with complete data") {
        SequenceProgress original;
        original.state = AppState::Off;
        original.current_step_index = 2;
        original.steps.push_back(StepProgress{
            .id = "step1",
            .display_name = "First Step",
            .status = StepStatus::Success,
            .message = "Completed successfully"
        });
        original.steps.push_back(StepProgress{
            .id = "step2",
            .display_name = "Second Step",
            .status = StepStatus::Failed,
            .message = "Network error"
        });
        original.last_error = "Step 2 failed: Network error";
        original.can_retry = true;
        original.abort_reason = AbortReason::UserRequested;

        nlohmann::json j = original;
        SequenceProgress restored = j.get<SequenceProgress>();

        REQUIRE(restored.state == original.state);
        REQUIRE(restored.current_step_index == original.current_step_index);
        REQUIRE(restored.steps.size() == original.steps.size());
        REQUIRE(restored.steps[0].id == original.steps[0].id);
        REQUIRE(restored.steps[1].status == original.steps[1].status);
        REQUIRE(restored.last_error == original.last_error);
        REQUIRE(restored.can_retry == original.can_retry);
        REQUIRE(restored.abort_reason == original.abort_reason);
    }
}

TEST_CASE("Startup | JSON - High-level serialization helpers", "[startup][json]") {
    using namespace startup;

    SECTION("serialize_result produces valid JSON string") {
        Result result{true, ""};
        std::string json_str = serialize_result(result);

        REQUIRE_FALSE(json_str.empty());

        // Parse it back to verify it's valid JSON
        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["ok"].get<bool>() == true);
    }

    SECTION("serialize_result handles error message") {
        Result result{false, "Something failed"};
        std::string json_str = serialize_result(result);

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["ok"].get<bool>() == false);
        REQUIRE(j["error"].get<std::string>() == "Something failed");
    }

    SECTION("serialize_step_config produces valid JSON string") {
        StepConfig config{
            .id = "test",
            .display_name = "Test",
            .timeout = std::chrono::seconds{10},
            .critical = true,
            .can_skip = false
        };

        std::string json_str = serialize_step_config(config);

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["id"].get<std::string>() == "test");
        REQUIRE(j["timeout"].get<int>() == 10);
    }

    SECTION("serialize_progress produces valid JSON string") {
        SequenceProgress progress;
        progress.state = AppState::Booting;
        progress.current_step_index = 0;
        progress.can_retry = false;
        progress.abort_reason = AbortReason::None;

        std::string json_str = serialize_progress(progress);

        REQUIRE_FALSE(json_str.empty());

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["state"].get<int>() == 1);
        REQUIRE(j["current_step_index"].get<int>() == 0);
    }

    SECTION("serialize_progress with complex data") {
        SequenceProgress progress;
        progress.state = AppState::Off;
        progress.current_step_index = 1;
        progress.steps.push_back(StepProgress{
            .id = "s1",
            .display_name = "Step 1",
            .status = StepStatus::Success,
            .message = "OK"
        });
        progress.steps.push_back(StepProgress{
            .id = "s2",
            .display_name = "Step 2",
            .status = StepStatus::Failed,
            .message = "Error"
        });
        progress.last_error = "Step 2 failed";
        progress.can_retry = true;
        progress.abort_reason = AbortReason::CriticalFailure;

        std::string json_str = serialize_progress(progress);

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["steps"].is_array());
        REQUIRE(j["steps"].size() == 2);
        REQUIRE(j["last_error"].get<std::string>() == "Step 2 failed");
    }
}

TEST_CASE("Startup | JSON - High-level deserialization helpers", "[startup][json]") {
    using namespace startup;

    SECTION("deserialize_result parses valid JSON") {
        std::string json_str = R"({"ok": true, "error": ""})";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == true);
        REQUIRE(result.ok == true);
        REQUIRE(result.error.empty());
    }

    SECTION("deserialize_result handles error") {
        std::string json_str = R"({"ok": false, "error": "Failed"})";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == true);
        REQUIRE(result.ok == false);
        REQUIRE(result.error == "Failed");
    }

    SECTION("deserialize_result returns error on invalid JSON") {
        std::string json_str = "not valid json {{{";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == false);
        REQUIRE_FALSE(parse_result.error.empty());
    }

    SECTION("deserialize_result returns error on missing fields") {
        std::string json_str = R"({"ok": true})";  // Missing "error" field
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == false);
    }

    SECTION("deserialize_step_config parses valid JSON") {
        std::string json_str = R"({
            "id": "test",
            "display_name": "Test Step",
            "timeout": 30,
            "critical": true,
            "can_skip": false
        })";
        StepConfig config;

        auto parse_result = deserialize_step_config(json_str, config);

        REQUIRE(parse_result.ok == true);
        REQUIRE(config.id == "test");
        REQUIRE(config.display_name == "Test Step");
        REQUIRE(config.timeout == std::chrono::seconds{30});
        REQUIRE(config.critical == true);
        REQUIRE(config.can_skip == false);
    }

    SECTION("deserialize_progress parses valid JSON") {
        std::string json_str = R"({
            "state": 1,
            "current_step_index": 0,
            "steps": [],
            "last_error": null,
            "can_retry": false,
            "abort_reason": 0
        })";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == true);
        REQUIRE(progress.state == AppState::Booting);
        REQUIRE(progress.current_step_index == 0);
        REQUIRE(progress.steps.empty());
    }

    SECTION("deserialize_progress with steps array") {
        std::string json_str = R"({
            "state": 2,
            "current_step_index": 1,
            "steps": [
                {
                    "id": "s1",
                    "display_name": "Step 1",
                    "status": 2,
                    "message": "Done",
                    "start_time": null,
                    "end_time": null
                }
            ],
            "last_error": "Some error",
            "can_retry": true,
            "abort_reason": 1
        })";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == true);
        REQUIRE(progress.state == AppState::Active);
        REQUIRE(progress.steps.size() == 1);
        REQUIRE(progress.steps[0].id == "s1");
        REQUIRE(progress.last_error.has_value());
        REQUIRE(progress.last_error.value() == "Some error");
        REQUIRE(progress.abort_reason == AbortReason::UserRequested);
    }

    SECTION("deserialize_progress returns error on invalid JSON") {
        std::string json_str = "invalid";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == false);
    }

    SECTION("round-trip via serialize/deserialize") {
        SequenceProgress original;
        original.state = AppState::Off;
        original.current_step_index = -1;
        original.can_retry = true;
        original.abort_reason = AbortReason::None;

        std::string json_str = serialize_progress(original);
        SequenceProgress restored;
        auto parse_result = deserialize_progress(json_str, restored);

        REQUIRE(parse_result.ok == true);
        REQUIRE(restored.state == original.state);
        REQUIRE(restored.current_step_index == original.current_step_index);
        REQUIRE(restored.can_retry == original.can_retry);
    }

    SECTION("deserialize_progress returns type_error on wrong field type") {
        // state should be int, not string
        std::string json_str = R"({
            "state": "invalid",
            "current_step_index": 0,
            "steps": [],
            "last_error": null,
            "can_retry": false,
            "abort_reason": 0
        })";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == false);
        REQUIRE(parse_result.error.find("type error") != std::string::npos);
    }

    SECTION("deserialize_progress returns out_of_range on missing field") {
        // Missing "state" field
        std::string json_str = R"({
            "current_step_index": 0,
            "steps": [],
            "last_error": null,
            "can_retry": false,
            "abort_reason": 0
        })";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == false);
        REQUIRE(parse_result.error.find("missing field") != std::string::npos);
    }

    SECTION("deserialize_result returns type_error on wrong field type") {
        // ok should be bool, not string
        std::string json_str = R"({"ok": "not_a_bool", "error": ""})";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == false);
        REQUIRE(parse_result.error.find("type error") != std::string::npos);
    }

    SECTION("deserialize_result returns out_of_range on missing field") {
        // Missing "error" field
        std::string json_str = R"({"ok": true})";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == false);
        REQUIRE(parse_result.error.find("missing field") != std::string::npos);
    }

    SECTION("deserialize_step_config returns type_error on wrong field type") {
        // timeout should be int, not string
        std::string json_str = R"({
            "id": "test",
            "display_name": "Test Step",
            "timeout": "not_a_number",
            "critical": true,
            "can_skip": false
        })";
        StepConfig config;

        auto parse_result = deserialize_step_config(json_str, config);

        REQUIRE(parse_result.ok == false);
        REQUIRE(parse_result.error.find("type error") != std::string::npos);
    }

    SECTION("deserialize_step_config returns out_of_range on missing field") {
        // Missing "id" field
        std::string json_str = R"({
            "display_name": "Test Step",
            "timeout": 30,
            "critical": true,
            "can_skip": false
        })";
        StepConfig config;

        auto parse_result = deserialize_step_config(json_str, config);

        REQUIRE(parse_result.ok == false);
        REQUIRE(parse_result.error.find("missing field") != std::string::npos);
    }
}
