#pragma once

int min(int a, int b) { return (a < b) * a + (b < a) * b; }
int sign(int a) { return -1 * (a < 0) + 1 * (a > 0); }
