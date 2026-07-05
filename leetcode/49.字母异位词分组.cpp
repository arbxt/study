/*
 * @lc app=leetcode.cn id=49 lang=cpp
 *
 * [49] 字母异位词分组
 */

// @lc code=start
class Solution {
public:
  vector<vector<string>> groupAnagrams(vector<string> &strs) {
    unordered_map<string, vector<string>> groups;

    for (const auto &s : strs) {
      string key = s;

      sort(key.begin(), key.end());

      groups[key].push_back(s);
    }

    vector<vector<string>> result;

    for (auto [key, group] : groups) {
      result.push_back(group);
    }

    return result;
  }
};
// @lc code=end
