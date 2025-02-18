#pragma once

int min(int a, int b) { return (a < b) * a + (b < a) * b; }
