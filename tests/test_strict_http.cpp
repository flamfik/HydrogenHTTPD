#include "StrictHttp.hpp"
#include <cassert>
#include <iostream>

int main() {
    HttpParseLimits limits;

    auto ok = StrictHttpParser::parse("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", limits);
    assert(ok.ok);
    assert(ok.request.headers.at("host") == "localhost");

    auto missingHost = StrictHttpParser::parse("GET / HTTP/1.1\r\n\r\n", limits);
    assert(!missingHost.ok);
    assert(missingHost.status == 400);

    auto duplicateHost = StrictHttpParser::parse("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", limits);
    assert(!duplicateHost.ok);

    auto transferEncoding = StrictHttpParser::parse("GET / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n", limits);
    assert(!transferEncoding.ok);
    assert(transferEncoding.status == 501);

    auto tooLongUri = StrictHttpParser::parse("GET /" + std::string(3000, 'a') + " HTTP/1.1\r\nHost: localhost\r\n\r\n", limits);
    assert(!tooLongUri.ok);
    assert(tooLongUri.status == 414);

    auto badHeader = StrictHttpParser::parse("GET / HTTP/1.1\r\nHost localhost\r\n\r\n", limits);
    assert(!badHeader.ok);

    auto body = StrictHttpParser::parse("POST /submit HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Type: text/plain\r\n\r\nhello", limits);
    assert(body.ok);
    assert(body.request.contentLength == 5);
    assert(body.request.body == "hello");

    auto tooLarge = StrictHttpParser::parse("POST /submit HTTP/1.1\r\nHost: localhost\r\nContent-Length: 999999999\r\n\r\n", limits);
    assert(!tooLarge.ok);
    assert(tooLarge.status == 413);

    auto incompleteBody = StrictHttpParser::parse("POST /submit HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhi", limits);
    assert(!incompleteBody.ok);
    assert(incompleteBody.status == 400);

    std::cout << "strict HTTP parser tests passed\n";
    return 0;
}
