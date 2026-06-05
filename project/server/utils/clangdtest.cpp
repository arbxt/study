#include <iostream>
#include <vector>
using namespace std;
int main() {
  vector<int> nums = {1, 2, 3};
  for (auto x : nums) {
    cout << x << endl;
  }
  return 0;
}