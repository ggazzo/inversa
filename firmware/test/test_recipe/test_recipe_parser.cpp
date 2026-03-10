// test_recipe_parser.cpp — Unit tests for recipe parsing
#include <unity.h>
#include "test_mocks.h"
#include <vector>

// Recipe command types (copied from RecipePlugin.h)
enum class RecipeCommandType : uint8_t {
    SetTemp,
    WaitTemp,
    WaitTimer,
    WaitConfirm,
    PumpOn,
    PumpOff,
    Comment,
    Unknown
};

struct RecipeCommand {
    RecipeCommandType type = RecipeCommandType::Unknown;
    float value = 0;
    String message = "";
};

// Recipe parser functions (extracted from RecipePlugin)
float extractFloat(const String& line, int offset) {
    String rest = line.substring(offset);
    rest.trim();
    return rest.toFloat();
}

String extractQuotedString(const String& line, int offset) {
    String rest = line.substring(offset);
    rest.trim();
    int q1 = rest.indexOf('"');
    if (q1 < 0) return rest;
    int q2 = rest.indexOf('"', q1 + 1);
    if (q2 < 0) return rest.substring(q1 + 1);
    return rest.substring(q1 + 1, q2);
}

RecipeCommand parseLine(const String& line) {
    RecipeCommand cmd;

    if (line.startsWith("#")) {
        cmd.type = RecipeCommandType::Comment;
        return cmd;
    }

    String upper = line;
    upper.toUpperCase();

    if (upper.startsWith("SET_TEMP")) {
        cmd.type = RecipeCommandType::SetTemp;
        cmd.value = extractFloat(line, 8);
    }
    else if (upper.startsWith("WAIT_TEMP")) {
        cmd.type = RecipeCommandType::WaitTemp;
        float tol = extractFloat(line, 9);
        cmd.value = (tol > 0) ? tol : 0.5f;
    }
    else if (upper.startsWith("WAIT_TIMER")) {
        cmd.type = RecipeCommandType::WaitTimer;
        cmd.value = extractFloat(line, 10);
    }
    else if (upper.startsWith("WAIT_CONFIRM")) {
        cmd.type = RecipeCommandType::WaitConfirm;
        cmd.message = extractQuotedString(line, 12);
        if (cmd.message.isEmpty()) {
            cmd.message = "Confirm to continue";
        }
    }
    else if (upper.startsWith("PUMP_ON")) {
        cmd.type = RecipeCommandType::PumpOn;
    }
    else if (upper.startsWith("PUMP_OFF")) {
        cmd.type = RecipeCommandType::PumpOff;
    }
    else {
        cmd.type = RecipeCommandType::Unknown;
    }

    return cmd;
}

std::vector<RecipeCommand> parseRecipe(const String& content) {
    std::vector<RecipeCommand> commands;
    int start = 0;
    
    while (start < (int)content.length()) {
        int end = content.indexOf('\n', start);
        if (end < 0) end = content.length();

        String line = content.substring(start, end);
        line.trim();
        start = end + 1;

        if (line.length() == 0) continue;

        RecipeCommand cmd = parseLine(line);
        if (cmd.type != RecipeCommandType::Comment && 
            cmd.type != RecipeCommandType::Unknown) {
            commands.push_back(cmd);
        }
    }
    
    return commands;
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
    TEST_ASSERT_TRUE(cmd.message == "Confirm to continue");
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
    
    return UNITY_END();
}
