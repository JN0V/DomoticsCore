// TEST-7: ComponentConfig's documented contract, against Arduino String
// semantics (the stub's toInt()/toFloat() are atol()/atof() since this lot).
// ComponentConfig.h is included first on purpose: the header must stand alone.
#include <DomoticsCore/ComponentConfig.h>
#include <unity.h>
#include <cmath>

using namespace DomoticsCore::Components;

void setUp(void) {}
void tearDown(void) {}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-6f; }

static ValidationResult validateOne(ConfigParam param, const char* value) {
    ComponentConfig cfg;
    cfg.defineParameter(param);
    cfg.setValue(param.name, value);
    return cfg.validate();
}

static void assertRefused(const ValidationResult& r, const char* message, const char* name) {
    TEST_ASSERT_FALSE(r.isValid());
    TEST_ASSERT_EQUAL(ComponentStatus::ConfigError, r.status);
    TEST_ASSERT_EQUAL_STRING(message, r.errorMessage.c_str());
    TEST_ASSERT_EQUAL_STRING(name, r.parameterName.c_str());
}

// ---- ComponentStatus, ComponentMetadata, ConfigParam ----------------------

void test_status_to_string_names_all_nine_and_the_unknown(void) {
    TEST_ASSERT_EQUAL_STRING("Success", statusToString(ComponentStatus::Success));
    TEST_ASSERT_EQUAL_STRING("Configuration Error", statusToString(ComponentStatus::ConfigError));
    TEST_ASSERT_EQUAL_STRING("Hardware Error", statusToString(ComponentStatus::HardwareError));
    TEST_ASSERT_EQUAL_STRING("Dependency Error", statusToString(ComponentStatus::DependencyError));
    TEST_ASSERT_EQUAL_STRING("Network Error", statusToString(ComponentStatus::NetworkError));
    TEST_ASSERT_EQUAL_STRING("Memory Error", statusToString(ComponentStatus::MemoryError));
    TEST_ASSERT_EQUAL_STRING("Timeout Error", statusToString(ComponentStatus::TimeoutError));
    TEST_ASSERT_EQUAL_STRING("Invalid State", statusToString(ComponentStatus::InvalidState));
    TEST_ASSERT_EQUAL_STRING("Not Supported", statusToString(ComponentStatus::NotSupported));
    TEST_ASSERT_EQUAL_STRING("Unknown Error", statusToString(static_cast<ComponentStatus>(99)));
}

void test_component_metadata_defaults_and_constructor(void) {
    ComponentMetadata d;
    TEST_ASSERT_EQUAL_STRING("", d.name);
    TEST_ASSERT_EQUAL_STRING("1.0.0", d.version);
    TEST_ASSERT_EQUAL_STRING("", d.author);
    TEST_ASSERT_EQUAL_STRING("", d.description);
    TEST_ASSERT_EQUAL_STRING("", d.category);
    TEST_ASSERT_TRUE(d.tags.empty());
    ComponentMetadata m("LED", "2.1.0", "me", "blinks");
    TEST_ASSERT_EQUAL_STRING("LED", m.name);
    TEST_ASSERT_EQUAL_STRING("2.1.0", m.version);
    TEST_ASSERT_EQUAL_STRING("me", m.author);
    TEST_ASSERT_EQUAL_STRING("blinks", m.description);
}

void test_config_param_fluent_chain_leaves_the_other_constraints_alone(void) {
    ConfigParam p("n", ConfigType::Integer);
    TEST_ASSERT_FALSE(p.required);
    TEST_ASSERT_EQUAL(INT_MIN, p.minValue);
    TEST_ASSERT_EQUAL(INT_MAX, p.maxValue);
    p.min(1).max(9);
    TEST_ASSERT_EQUAL(1, p.minValue);
    TEST_ASSERT_EQUAL(9, p.maxValue);
    TEST_ASSERT_EQUAL_size_t(0, p.maxLength);
    TEST_ASSERT_TRUE(p.allowedValues.empty());
    p.length(5).options({"a", "b"});
    TEST_ASSERT_EQUAL_size_t(5, p.maxLength);
    TEST_ASSERT_EQUAL_size_t(2, p.allowedValues.size());
    TEST_ASSERT_EQUAL(1, p.minValue);
    TEST_ASSERT_EQUAL(9, p.maxValue);
}

// ---- values ---------------------------------------------------------------

void test_define_parameter_sets_the_value_only_when_a_default_is_given(void) {
    ComponentConfig cfg;
    cfg.defineParameter(ConfigParam("with", ConfigType::String, false, "x"));
    cfg.defineParameter(ConfigParam("without", ConfigType::String));
    TEST_ASSERT_EQUAL_STRING("x", cfg.getValue("with").c_str());
    TEST_ASSERT_EQUAL_STRING("dflt", cfg.getValue("without", "dflt").c_str());
    TEST_ASSERT_EQUAL_STRING("", cfg.getValue("never").c_str());
    TEST_ASSERT_EQUAL_size_t(2, cfg.getParameters().size());
}

void test_typed_getters_read_set_values(void) {
    ComponentConfig cfg;
    cfg.setValue("i", "42");
    cfg.setValue("f", "1.5");
    cfg.setValue("b", "true");
    TEST_ASSERT_EQUAL(42, cfg.getInt("i"));
    TEST_ASSERT_TRUE(near(1.5f, cfg.getFloat("f")));
    TEST_ASSERT_TRUE(cfg.getBool("b"));
}

// A set-but-garbage value parses to zero/false; only an unset one falls back.
void test_typed_getters_fall_back_only_when_the_value_is_unset(void) {
    ComponentConfig cfg;
    TEST_ASSERT_EQUAL(7, cfg.getInt("k", 7));
    TEST_ASSERT_TRUE(near(2.5f, cfg.getFloat("k", 2.5f)));
    TEST_ASSERT_TRUE(cfg.getBool("k", true));
    cfg.setValue("k", "abc");
    TEST_ASSERT_EQUAL(0, cfg.getInt("k", 7));
    TEST_ASSERT_TRUE(near(0.0f, cfg.getFloat("k", 2.5f)));
    TEST_ASSERT_FALSE(cfg.getBool("k", true));
}

void test_get_bool_takes_the_eight_tokens_regardless_of_case(void) {
    ComponentConfig cfg;
    const char* yes[] = {"true", "1", "yes", "on", "TRUE", "Yes", "ON"};
    for (const char* v : yes) { cfg.setValue("b", v); TEST_ASSERT_TRUE_MESSAGE(cfg.getBool("b"), v); }
    const char* no[] = {"false", "0", "no", "off", "FALSE", "Off"};
    for (const char* v : no) { cfg.setValue("b", v); TEST_ASSERT_FALSE_MESSAGE(cfg.getBool("b"), v); }
}

void test_has_parameter_is_about_values_not_definitions(void) {
    ComponentConfig cfg;
    cfg.defineParameter(ConfigParam("defined", ConfigType::String));
    TEST_ASSERT_FALSE(cfg.hasParameter("defined"));
    cfg.setValue("defined", "v");
    TEST_ASSERT_TRUE(cfg.hasParameter("defined"));
    cfg.setValue("undeclared", "v");
    TEST_ASSERT_TRUE(cfg.hasParameter("undeclared"));
}

// ---- validate() -----------------------------------------------------------

void test_validate_reports_a_required_parameter_missing(void) {
    ComponentConfig cfg;
    cfg.defineParameter(ConfigParam("host", ConfigType::String, true));
    assertRefused(cfg.validate(), "Required parameter missing", "host");
}

void test_validate_skips_an_empty_optional_parameter(void) {
    ComponentConfig cfg;
    cfg.defineParameter(ConfigParam("port", ConfigType::Port));
    TEST_ASSERT_TRUE(cfg.validate().isValid());
    TEST_ASSERT_EQUAL_STRING("Valid", cfg.validate().toString().c_str());
}

void test_validate_reports_the_first_failing_parameter_in_definition_order(void) {
    ComponentConfig cfg;
    cfg.defineParameter(ConfigParam("a", ConfigType::Integer, false, "1"));
    cfg.defineParameter(ConfigParam("b", ConfigType::Boolean, false, "maybe"));
    cfg.defineParameter(ConfigParam("c", ConfigType::Port, false, "0"));
    assertRefused(cfg.validate(), "Invalid boolean format", "b");
}

void test_validate_boolean_tokens_and_a_refusal(void) {
    ConfigParam p("b", ConfigType::Boolean);
    TEST_ASSERT_TRUE(validateOne(p, "off").isValid());
    TEST_ASSERT_TRUE(validateOne(p, "Yes").isValid());
    TEST_ASSERT_TRUE(validateOne(p, "0").isValid());
    assertRefused(validateOne(p, "maybe"), "Invalid boolean format", "b");
}

void test_validate_string_length_and_options(void) {
    ConfigParam bounded("s", ConfigType::String); bounded.length(3);
    TEST_ASSERT_TRUE(validateOne(bounded, "abc").isValid());
    assertRefused(validateOne(bounded, "abcd"), "String too long", "s");
    ConfigParam listed("s", ConfigType::String); listed.options({"a", "b"});
    TEST_ASSERT_TRUE(validateOne(listed, "a").isValid());
    assertRefused(validateOne(listed, "c"), "Value not in allowed list", "s");
}

void test_validate_integer_range_is_inclusive_at_both_ends(void) {
    ConfigParam p("i", ConfigType::Integer); p.min(1).max(9);
    TEST_ASSERT_TRUE(validateOne(p, "1").isValid());
    TEST_ASSERT_TRUE(validateOne(p, "9").isValid());
    assertRefused(validateOne(p, "0"), "Value out of range", "i");
    assertRefused(validateOne(p, "10"), "Value out of range", "i");
}

void test_validate_port_range_at_both_ends(void) {
    ConfigParam p("p", ConfigType::Port);
    TEST_ASSERT_TRUE(validateOne(p, "1").isValid());
    TEST_ASSERT_TRUE(validateOne(p, "65535").isValid());
    assertRefused(validateOne(p, "0"), "Port out of range (1-65535)", "p");
    assertRefused(validateOne(p, "65536"), "Port out of range (1-65535)", "p");
}

void test_validate_ip_address_shape_and_range(void) {
    ConfigParam p("ip", ConfigType::IPAddress);
    TEST_ASSERT_TRUE(validateOne(p, "192.168.1.1").isValid());
    TEST_ASSERT_TRUE(validateOne(p, "0.0.0.0").isValid());
    assertRefused(validateOne(p, "256.1.1.1"), "Invalid IP address range", "ip");
    assertRefused(validateOne(p, "1.2.3"), "Invalid IP address format", "ip");
    assertRefused(validateOne(p, "1.2.3.4.5"), "Invalid IP address format", "ip");
    assertRefused(validateOne(p, "1.2.3.4."), "Invalid IP address format", "ip");
    assertRefused(validateOne(p, "1..2.3"), "Invalid IP address format", "ip");
}

void test_validation_result_to_string_three_shapes(void) {
    TEST_ASSERT_EQUAL_STRING("Valid", ValidationResult().toString().c_str());
    TEST_ASSERT_EQUAL_STRING("Configuration Error",
                             ValidationResult(ComponentStatus::ConfigError).toString().c_str());
    TEST_ASSERT_EQUAL_STRING("Configuration Error (name)",
                             ValidationResult(ComponentStatus::ConfigError, "", "name").toString().c_str());
    TEST_ASSERT_EQUAL_STRING("Configuration Error (name): bad",
                             ValidationResult(ComponentStatus::ConfigError, "bad", "name").toString().c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_status_to_string_names_all_nine_and_the_unknown);
    RUN_TEST(test_component_metadata_defaults_and_constructor);
    RUN_TEST(test_config_param_fluent_chain_leaves_the_other_constraints_alone);
    RUN_TEST(test_define_parameter_sets_the_value_only_when_a_default_is_given);
    RUN_TEST(test_typed_getters_read_set_values);
    RUN_TEST(test_typed_getters_fall_back_only_when_the_value_is_unset);
    RUN_TEST(test_get_bool_takes_the_eight_tokens_regardless_of_case);
    RUN_TEST(test_has_parameter_is_about_values_not_definitions);
    RUN_TEST(test_validate_reports_a_required_parameter_missing);
    RUN_TEST(test_validate_skips_an_empty_optional_parameter);
    RUN_TEST(test_validate_reports_the_first_failing_parameter_in_definition_order);
    RUN_TEST(test_validate_boolean_tokens_and_a_refusal);
    RUN_TEST(test_validate_string_length_and_options);
    RUN_TEST(test_validate_integer_range_is_inclusive_at_both_ends);
    RUN_TEST(test_validate_port_range_at_both_ends);
    RUN_TEST(test_validate_ip_address_shape_and_range);
    RUN_TEST(test_validation_result_to_string_three_shapes);
    return UNITY_END();
}
