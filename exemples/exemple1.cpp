#include <stdio.h>
#include "../curl-wrapper.h"

int main(int argc, char const *argv[])
{
    Curl::Initialize();

    Curl::CurlEasy Client;
    Client.SetUrl("http://example.com");
    Client.SetTimeoutMs(1000);

    Client.Perform();

    std::string& header = Client.GetHeader();
    printf("%s\n", header.c_str());

    std::string& body = Client.GetContent();
    printf("%s\n", body.c_str());

    Curl::Shutdown();

    return 0;
}