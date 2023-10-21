#pragma once

#include <string>
#include <vector>

std::vector<std::u16string> parse_unicode_text_set(const std::string& prs_data);
std::string serialize_unicode_text_set(const std::vector<std::u16string>& strings);
