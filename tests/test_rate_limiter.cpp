#include "RateLimiter.hpp"
#include <cassert>
#include <iostream>
int main() {
    RateLimiter limiter(2);
    assert(limiter.allow("a"));
    assert(limiter.allow("a"));
    assert(!limiter.allow("a"));
    RateLimiter disabled(0);
    assert(!disabled.allow("x"));
    std::cout << "rate limiter tests passed\n";
    return 0;
}
