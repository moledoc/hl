#pragma once

int min(int a, int b) { return (a < b) * a + (b < a) * b; }
int sign(int a) { return -1 * (a < 0) + 1 * (a > 0); }
int clamp(int target, int lower_bound, int upper_bound) {
  return target * (lower_bound <= target && target <= upper_bound) +
         lower_bound * (target < lower_bound) +
         upper_bound * (upper_bound < target);
}
int gt(int a, int b) { return (a >= b) * a + (a < b) * b; }
int lt(int a, int b) { return (a <= b) * a + (a > b) * b; }