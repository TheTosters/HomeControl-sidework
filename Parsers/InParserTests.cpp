/*
 * InParserTests.cpp
 *
 *  Created on: Mar 27, 2017
 *      Author: Zarnowski
 */
/*
 * MiniInParserTests.cpp
 *
 *  Created on: Mar 23, 2017
 *      Author: Zarnowski
 */

#include "InParserTests.hpp"
#include "InParser.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static bool testNoParamCmd() {
    shared_ptr<RemoteCommand> cmd;
    bool testResult;
    InParser parser;

    cmd = parser.parse( make_shared<string>("CMD"));
    testResult = (cmd != nullptr) && cmd->getArgType() == RemoteCommandArgumentType_NONE;
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd

    //invalid
    //no " starting string
    cmd = parser.parse( make_shared<string>("CMDinv"));
    testResult &= cmd == nullptr;

    //no capital letters used
    cmd = parser.parse( make_shared<string>("CmD"));
    testResult &= cmd == nullptr;

    //numbers not allowed here
    cmd = parser.parse( make_shared<string>("C1D"));
    testResult &= cmd == nullptr;

    //to short command
    cmd = parser.parse( make_shared<string>("C"));
    testResult &= cmd == nullptr;

    //too short command
    cmd = parser.parse( make_shared<string>("CM"));
    testResult &= cmd == nullptr;

    //no command
    cmd = parser.parse( make_shared<string>(""));
    testResult &= cmd == nullptr;

    return testResult;
}

static bool testStringMultiListParamCmd() {
  shared_ptr<RemoteCommand> cmd;
  bool testResult = true;
  InParser parser;
  
  cmd = parser.parse( make_shared<string>("CMD(\"te\",\"aaa\",\"ttte\")(\"asas\")"));
  testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_STRING_MULTI_SEQUENCE);
  testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 2);
  testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd
  
  if (cmd != nullptr) {
    shared_ptr<vector<shared_ptr<string> > > tmp = cmd->getStringSequence();
    
    testResult &= strcmp("te", (*tmp)[0]->c_str()) == 0;
    testResult &= strcmp("aaa", (*tmp)[1]->c_str()) == 0;
    testResult &= strcmp("ttte", (*tmp)[2]->c_str()) == 0;
    
    tmp = cmd->getStringSequence(1);
    testResult &= strcmp("asas", (*tmp)[0]->c_str()) == 0;
  }
  
  //no ',' between ) (
  cmd = parser.parse( make_shared<string>("CMD(\"ll\"),(\"ll3\")"));
  testResult &= cmd == nullptr;
  
  //no mixing digit/string sequences
  cmd = parser.parse( make_shared<string>("CMD(\"bad\")(1,2,3)"));
  testResult &= cmd == nullptr;
  
  //only sub sequences allowed
  cmd = parser.parse( make_shared<string>("CMD(\"as\")\"bad\""));
  testResult &= cmd == nullptr;
  
  //only sub sequences allowed again
  cmd = parser.parse( make_shared<string>("CMD(\"as\"),\"bad\""));
  testResult &= cmd == nullptr;
  
  //No empty () allowed
  cmd = parser.parse( make_shared<string>("CMD(\"as\")()"));
  testResult &= cmd == nullptr;
  
  return testResult;
}

static bool testDigitMultiListParamCmd() {
  shared_ptr<RemoteCommand> cmd;
  bool testResult = true;
  InParser parser;
  const double epsilon = 1.0 / 255.0;
  
  cmd = parser.parse( make_shared<string>("CMD(123,22.33,-12)(0)"));
  testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT_MULTI_SEQUENCE);
  testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 2);
  testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd

  if (cmd != nullptr) {
      shared_ptr<vector<Number> > tmp = cmd->getDigitSequence(0);
      testResult &= (*tmp)[0].asInt() == 123;
      testResult &= (fabs((*tmp)[1].asDouble() - 22.33) <= epsilon);
      testResult &= (*tmp)[2].asInt() == -12;
      
      tmp = cmd->getDigitSequence(1);
      testResult &= (*tmp)[0].asInt() == 0;
  }
  //no ',' between ) (
  cmd = parser.parse( make_shared<string>("CMD(1,2),(1,22)"));
  testResult &= cmd == nullptr;
  
  //no mixing digit/string sequences
  cmd = parser.parse( make_shared<string>("CMD(1,2)(\"bad\")"));
  testResult &= cmd == nullptr;
  
  //only sub sequences allowed
  cmd = parser.parse( make_shared<string>("CMD(1,2)3"));
  testResult &= cmd == nullptr;
  
  //only sub sequences allowed again
  cmd = parser.parse( make_shared<string>("CMD(1,2),3"));
  testResult &= cmd == nullptr;
  
  //No empty () allowed
  cmd = parser.parse( make_shared<string>("CMD()(1,1)"));
  testResult &= cmd == nullptr;
  
  return testResult;
}

static bool testDigitListParamCmd() {
  shared_ptr<RemoteCommand> cmd;
  bool testResult = true;
  InParser parser;
  const double epsilon = 1.0 / 255.0;
  
  cmd = parser.parse( make_shared<string>("CMD123,22.33,-12,0"));
  testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT_SEQUENCE);
  testResult &= (cmd != nullptr) && (cmd->argumentAsInt() == 123);
  testResult &= (cmd != nullptr) && ((fabs(cmd->argumentAsDouble(1) - 22.33) <= epsilon));
  testResult &= (cmd != nullptr) && (cmd->argumentAsInt(2) == -12);
  testResult &= (cmd != nullptr) && (cmd->argumentAsInt(3) == 0);
  testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 4);
  testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd

  //no digit/string mixing allowed
  cmd = parser.parse( make_shared<string>("CMD123,\"illegal\",0"));
  testResult &= cmd == nullptr;

  //non digit character
  cmd = parser.parse( make_shared<string>("CMD123,bad,0"));
  testResult &= cmd == nullptr;

  //no empty digits allowed!
  cmd = parser.parse( make_shared<string>("CMD123,,0"));
  testResult &= cmd == nullptr;
  
  return testResult;
}

static bool testStringListParamCmd() {
  shared_ptr<RemoteCommand> cmd;
  bool testResult = true;
  InParser parser;
  
  cmd = parser.parse( make_shared<string>("CMD\"123\",\"test\",\"abc\""));
  testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_STRING_SEQUENCE);
  testResult &= (cmd != nullptr) && (strcmp("123", cmd->stringArgument()->c_str()) == 0);
  testResult &= (cmd != nullptr) && (strcmp("test", cmd->stringArgument(1)->c_str()) == 0);
  testResult &= (cmd != nullptr) && (strcmp("abc", cmd->stringArgument(2)->c_str()) == 0);
  testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 3);
  testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd
  
  //no digit/string mixing allowed
  cmd = parser.parse( make_shared<string>("CMD\"ok\",0,\"ok2\""));
  testResult &= cmd == nullptr;
  
  //no double commas allowed
  cmd = parser.parse( make_shared<string>("CMD\"ok\",,\"ok2\""));
  testResult &= cmd == nullptr;
  
  return testResult;
}

static bool testIntDigitParamCmd() {
    shared_ptr<RemoteCommand> cmd;
    bool testResult = true;
    InParser parser;

    cmd = parser.parse( make_shared<string>("CMD123"));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT);
    testResult &= (cmd != nullptr) && (cmd->argumentAsInt() == 123);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd

    //invalid character 'x'
    
    cmd = parser.parse( make_shared<string>("CMD12x2"));
    testResult &= cmd == nullptr;

    //no hex is supported
    cmd = parser.parse( make_shared<string>("CMD12a2"));
    testResult &= cmd == nullptr;

    //chack max 32 bits value
    cmd = parser.parse( make_shared<string>("CMD4294967295"));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT);
    testResult &= (cmd != nullptr) && (cmd->argumentAsInt() == 0xFFFFFFFF);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd

    //chack max 64 bits value
    cmd = parser.parse( make_shared<string>("CMD18446744073709551615"));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT);
    testResult &= (cmd != nullptr) && (cmd->argumentAsUInt() == 0xffffffffffffffff);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd


    //check -98 bits value
    cmd = parser.parse( make_shared<string>("CMD-98"));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT);
    testResult &= (cmd != nullptr) && (cmd->argumentAsInt() == -98);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd

    //'-' in wrong place
    cmd = parser.parse( make_shared<string>("CMD11-22"));
    testResult &= cmd == nullptr;

    //too many '-'
    cmd = parser.parse( make_shared<string>("CMD--1122"));
    testResult &= cmd == nullptr;

    return testResult;
}

static bool testStringParamCmd() {
    shared_ptr<RemoteCommand> cmd;
    bool testResult = true;
    InParser parser;

    cmd = parser.parse( make_shared<string>("CMD\"test\""));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_STRING);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd
    testResult &= (cmd != nullptr) && (strcmp("test", cmd->stringArgument()->c_str()) == 0);

    cmd = parser.parse( make_shared<string>("CMD\"012345678\""));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_STRING);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd
    testResult &= (cmd != nullptr) && (strcmp("012345678", cmd->stringArgument()->c_str()) == 0);

    //invalid character
    cmd = parser.parse( make_shared<string>("CMD\"\x10\""));
    testResult &= cmd == nullptr;

    //invalid character 3 x "
    cmd = parser.parse( make_shared<string>("CMD\"\"\""));
    testResult &= cmd == nullptr;

    //invalid, no last "
    cmd = parser.parse( make_shared<string>("CMD\"sdsad"));
    testResult &= cmd == nullptr;
                                            
    return testResult;
}

static bool testIntFixedParamCmd() {
    shared_ptr<RemoteCommand> cmd;
    bool testResult = true;
    InParser parser;
    const double epsilon = 1.0 / 255.0;
  

    cmd = parser.parse( make_shared<string>("CMD123.00"));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT);
    testResult &= (cmd != nullptr) && (fabs(cmd->argumentAsDouble() - 123) <= epsilon);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd
  
    cmd = parser.parse( make_shared<string>("CMD123.12"));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT);
    testResult &= (cmd != nullptr) && (fabs(cmd->argumentAsDouble() - 123.12) <= epsilon);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd

    //double '-' is failure
    cmd = parser.parse( make_shared<string>("CMD--123.12"));
    testResult &= cmd == nullptr;

    // '-' in wrong place
    cmd = parser.parse( make_shared<string>("CMD123.-12"));
    testResult &= cmd == nullptr;

    // illegal character
    cmd = parser.parse( make_shared<string>("CMD123.1d2"));
    testResult &= cmd == nullptr;

    // illegal character 2x '.'
    cmd = parser.parse( make_shared<string>("CMD123.1.2"));
    testResult &= cmd == nullptr;

    cmd = parser.parse( make_shared<string>("CMD-123.45"));
    testResult &= (cmd != nullptr) && (cmd->getArgType() == RemoteCommandArgumentType_DIGIT);
    testResult &= (cmd != nullptr) && (fabs(cmd->argumentAsDouble() - (-123.45)) <= epsilon);
    testResult &= (cmd != nullptr) && (cmd->argumentsCount() == 1);
    testResult &= (cmd != nullptr) && (*cmd == "CMD");   //cmd
  
    return testResult;
}

bool testInParser() {
    bool result = true;
    try {
        result &= testNoParamCmd();
        result &= testIntDigitParamCmd();
        result &= testIntFixedParamCmd();
        result &= testStringParamCmd();
      
        result &= testDigitListParamCmd();
        result &= testStringListParamCmd();
      
        result &= testDigitMultiListParamCmd();
        result &= testStringMultiListParamCmd();
    } catch (...) {
        return false;
    }
    return result;

}
