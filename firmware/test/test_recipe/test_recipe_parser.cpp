// test_recipe_parser.cpp — unit + conformance tests for the recipe DSL.
//
// Exercises the REAL parser in core/RecipeParser.h (shared with RecipePlugin —
// no copy to drift). The conformance tests pin the same sample recipes the app
// parser pins (packages/utils/src/parseRecipe.test.ts), so the two
// implementations can't diverge on the parsed step sequence.
#include <unity.h>
#include "test_mocks.h"          // lightweight String stub
#include <vector>
#include <fstream>
#include <sstream>
#include "../../src/core/RecipeParser.h"

using RecipeParser::parseLine;

// Mirror of RecipePlugin's load loop: keep non-comment/unknown commands.
static std::vector<RecipeCommand> parseRecipe(const String& content) {
    std::vector<RecipeCommand> commands;
    int start = 0;
    while (start < (int)content.length()) {
        int end = content.indexOf('\n', start);
        if (end < 0) end = (int)content.length();
        String line = content.substring(start, end);
        line.trim();
        start = end + 1;
        if (line.length() == 0) continue;
        RecipeCommand cmd = parseLine(line.c_str());
        if (cmd.type != RecipeCommandType::Comment &&
            cmd.type != RecipeCommandType::Unknown) {
            commands.push_back(cmd);
        }
    }
    return commands;
}

// Read a sample recipe relative to the firmware/ dir (the pio test cwd).
static String readSample(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return String(ss.str().c_str());
}

// ─── Tests ──────────────────────────────────────────────────

void test_parse_set_temp() {
    RecipeCommand cmd = parseLine("SET_TEMP 65.5");
    
    TEST_ASSERT_EQUAL(RecipeCommandType::SetTemp, cmd.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 65.5, cmd.value);
}

void test_parse_set_temp_lowercase() {
    RecipeCommand cmd = parseLine("set_temp 72");
    
    TEST_ASSERT_EQUAL(RecipeCommandType::SetTemp, cmd.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 72.0, cmd.value);
}

void test_parse_wait_temp() {
    RecipeCommand cmd = parseLine("WAIT_TEMP 1.0");
    
    TEST_ASSERT_EQUAL(RecipeCommandType::WaitTemp, cmd.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 1.0, cmd.value);
}

void test_parse_wait_temp_default_tolerance() {
    RecipeCommand cmd = parseLine("WAIT_TEMP");
    
    TEST_ASSERT_EQUAL(RecipeCommandType::WaitTemp, cmd.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 0.5, cmd.value);  // Default tolerance
}

void test_parse_wait_timer() {
    RecipeCommand cmd = parseLine("WAIT_TIMER 60");
    
    TEST_ASSERT_EQUAL(RecipeCommandType::WaitTimer, cmd.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, cmd.value);
}

void test_parse_wait_confirm_with_message() {
    RecipeCommand cmd = parseLine("WAIT_CONFIRM \"Add the hops\"");
    
    TEST_ASSERT_EQUAL(RecipeCommandType::WaitConfirm, cmd.type);
    TEST_ASSERT_TRUE(cmd.message == "Add the hops");
}

void test_parse_wait_confirm_without_message() {
    RecipeCommand cmd = parseLine("WAIT_CONFIRM");
    
    TEST_ASSERT_EQUAL(RecipeCommandType::WaitConfirm, cmd.type);
    TEST_ASSERT_TRUE(cmd.message == "Confirmar para continuar");  // real parser default (the old copy asserted a stale English string)
}

void test_parse_pump_on() {
    RecipeCommand cmd = parseLine("PUMP_ON");
    TEST_ASSERT_EQUAL(RecipeCommandType::PumpOn, cmd.type);
}

void test_parse_pump_off() {
    RecipeCommand cmd = parseLine("PUMP_OFF");
    TEST_ASSERT_EQUAL(RecipeCommandType::PumpOff, cmd.type);
}

void test_parse_comment() {
    RecipeCommand cmd = parseLine("# This is a comment");
    TEST_ASSERT_EQUAL(RecipeCommandType::Comment, cmd.type);
}

void test_parse_unknown() {
    RecipeCommand cmd = parseLine("INVALID_COMMAND 123");
    TEST_ASSERT_EQUAL(RecipeCommandType::Unknown, cmd.type);
}

void test_parse_full_recipe() {
    String recipe = 
        "# Mash schedule\n"
        "SET_TEMP 65\n"
        "WAIT_TEMP\n"
        "PUMP_ON\n"
        "WAIT_TIMER 60\n"
        "SET_TEMP 72\n"
        "WAIT_TEMP 0.5\n"
        "WAIT_TIMER 10\n"
        "PUMP_OFF\n"
        "WAIT_CONFIRM \"Ready to sparge\"\n";
    
    auto commands = parseRecipe(recipe);
    
    TEST_ASSERT_EQUAL(9, commands.size());
    TEST_ASSERT_EQUAL(RecipeCommandType::SetTemp, commands[0].type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 65.0, commands[0].value);
    TEST_ASSERT_EQUAL(RecipeCommandType::WaitTemp, commands[1].type);
    TEST_ASSERT_EQUAL(RecipeCommandType::PumpOn, commands[2].type);
    TEST_ASSERT_EQUAL(RecipeCommandType::WaitTimer, commands[3].type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, commands[3].value);
}

void test_parse_empty_lines_skipped() {
    String recipe = 
        "SET_TEMP 65\n"
        "\n"
        "   \n"
        "WAIT_TEMP\n";
    
    auto commands = parseRecipe(recipe);
    
    TEST_ASSERT_EQUAL(2, commands.size());
}

// ─── Conformance: shipped sample recipes ────────────────────
// Pins the same {type, value, message} sequence the app parser pins in
// packages/utils/src/parseRecipe.test.ts. If the firmware parser drifts from
// that contract, this fails. (STEP→BrewingStep mapping lives in RecipePlugin,
// out of the cross-parser contract; STEP here only carries the label.)

void test_conformance_ipa() {
    String c = readSample("samples/IPA.txt");
    TEST_ASSERT_TRUE_MESSAGE(c.length() > 0, "samples/IPA.txt not found (run from firmware/)");
    auto cmd = parseRecipe(c);
    TEST_ASSERT_EQUAL(19, cmd.size());

    const RecipeCommandType expect[19] = {
        RecipeCommandType::Step, RecipeCommandType::SetTemp, RecipeCommandType::WaitTemp,
        RecipeCommandType::Step, RecipeCommandType::WaitTimer, RecipeCommandType::MashOut,
        RecipeCommandType::WaitTemp, RecipeCommandType::Step, RecipeCommandType::WaitConfirm,
        RecipeCommandType::Step, RecipeCommandType::SetTemp, RecipeCommandType::WaitTemp,
        RecipeCommandType::AddHop, RecipeCommandType::AddHop, RecipeCommandType::AddHop,
        RecipeCommandType::Boil, RecipeCommandType::WaitBoil, RecipeCommandType::Step,
        RecipeCommandType::HeaterOff,
    };
    for (int i = 0; i < 19; i++) TEST_ASSERT_EQUAL(expect[i], cmd[i].type);

    // Spot-check values/messages on the value-bearing steps.
    TEST_ASSERT_FLOAT_WITHIN(0.1, 67.0, cmd[1].value);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, cmd[4].value);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 76.0, cmd[5].value);
    TEST_ASSERT_TRUE(cmd[8].message == "Iniciar lavagem com 10L");
    TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, cmd[12].value);
    TEST_ASSERT_TRUE(cmd[12].message == "Magnum 30g");
    TEST_ASSERT_TRUE(cmd[14].message == "Citra 30g");
    TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, cmd[15].value);
    TEST_ASSERT_TRUE(cmd[0].message == "Pre-aquecimento");
}

void test_conformance_quick() {
    String c = readSample("samples/quick.txt");
    TEST_ASSERT_TRUE_MESSAGE(c.length() > 0, "samples/quick.txt not found");
    auto cmd = parseRecipe(c);
    TEST_ASSERT_EQUAL(7, cmd.size());
    const RecipeCommandType expect[7] = {
        RecipeCommandType::Step, RecipeCommandType::SetTemp, RecipeCommandType::WaitTemp,
        RecipeCommandType::Step, RecipeCommandType::WaitTimer, RecipeCommandType::Step,
        RecipeCommandType::HeaterOff,
    };
    for (int i = 0; i < 7; i++) TEST_ASSERT_EQUAL(expect[i], cmd[i].type);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, cmd[1].value);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 1.0, cmd[2].value);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 1.0, cmd[4].value);
}

// ─── Main ───────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_parse_set_temp);
    RUN_TEST(test_parse_set_temp_lowercase);
    RUN_TEST(test_parse_wait_temp);
    RUN_TEST(test_parse_wait_temp_default_tolerance);
    RUN_TEST(test_parse_wait_timer);
    RUN_TEST(test_parse_wait_confirm_with_message);
    RUN_TEST(test_parse_wait_confirm_without_message);
    RUN_TEST(test_parse_pump_on);
    RUN_TEST(test_parse_pump_off);
    RUN_TEST(test_parse_comment);
    RUN_TEST(test_parse_unknown);
    RUN_TEST(test_parse_full_recipe);
    RUN_TEST(test_parse_empty_lines_skipped);
    RUN_TEST(test_conformance_ipa);
    RUN_TEST(test_conformance_quick);

    return UNITY_END();
}
