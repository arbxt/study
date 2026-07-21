#include <iostream>
#include <string>

namespace {
std::string escape_json_string(const std::string &input) {
  std::string result;

  for (char c : input) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;

    case '\\':
      result += "\\\\";
      break;

    case '\n':
      result += "\\n";
      break;

    case '\r':
      result += "\\r";
      break;

    case '\t':
      result += "\\t";
      break;

    default:
      result += c;
      break;
    }
  }

  return result;
}

std::string unescape_json_string(const std::string &input) {
  std::string result;

  for (size_t i = 0; i < input.size(); i++) {
    char c = input[i];

    if (c != '\\') {
      result += c;
      continue;
    }

    // 遇到转义符
    if (i + 1 >= input.size()) {
      return {};
    }

    char next = input[++i];

    switch (next) {
    case '"':
      result += '"';
      break;

    case '\\':
      result += '\\';
      break;

    case 'n':
      result += '\n';
      break;

    case 'r':
      result += '\r';
      break;

    case 't':
      result += '\t';
      break;

    default:
      // 非法转义
      return {};
    }
  }

  return result;
}
} // namespace
void test_escape(const std::string &input) {
  std::string escaped = escape_json_string(input);

  std::string unescaped = unescape_json_string(escaped);

  std::cout << "input:      ";
  std::cout << input << "\n";

  std::cout << "escaped:    ";
  std::cout << escaped << "\n";

  std::cout << "unescaped:  ";
  std::cout << unescaped << "\n";

  if (input == unescaped) {
    std::cout << "[PASS]\n";
  } else {
    std::cout << "[FAIL]\n";
  }

  std::cout << "----------------\n";
}

void test_invalid() {
  std::string s = "hello\\q";

  auto result = unescape_json_string(s);

  if (result.empty()) {
    std::cout << "[PASS] invalid escape\n";
  }
}

int main() {
  test_escape("hello world");

  test_escape("hello\"world");

  test_escape("hello\\world");

  test_escape("hello\nworld");

  test_escape("hello\tworld");

  test_escape("hello\rworld");

  test_invalid();
  return 0;
}