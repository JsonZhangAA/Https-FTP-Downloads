#pragma once

#include "advanDown.h"

int main(int argc,char * * argv)
{
    HttpDownFile  cHDown(argv[1],argv[2]);

    cHDown.downFile();
    return 0;
}