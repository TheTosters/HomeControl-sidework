//
//  main.cpp
//  Parsers
//
//  Created by Bartłomiej on 23/03/17.
//  Copyright © 2017 Imagination Systems. All rights reserved.
//

#include <iostream>
#include "MiniInParserTests.h"
#include "InParserTests.hpp"
#include "RemoteCommandBuilderTests.hpp"

int main(int argc, const char * argv[]) {
    if (testMiniInParser() == true) {
      printf("MiniParser: SUCCESS\n");
    } else {
      printf("MiniParser: FAILURE\n");
    }
    if (testInParser() == true) {
      printf("Parser: SUCCESS\n");
    } else {
      printf("Parser: FAILURE\n");
    }
    if (testRemoteCommandBuilder() == true) {
        printf("RemoteCommandBuilder: SUCCESS\n");
    } else {
        printf("RemoteCommandBuilder: FAILURE\n");
    }
    return 0;
}
