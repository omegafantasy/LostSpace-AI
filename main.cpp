//
// Created by Emily Jia on 2021/2/27.
//
#include "ai_client.h"

int main(int argc, char** argv)
{
    std::string map_path = argc!=2? "mapconf2.map": argv[1];
    AI_Client ai(map_path.c_str());
    ai.run();
    return 0;
}
