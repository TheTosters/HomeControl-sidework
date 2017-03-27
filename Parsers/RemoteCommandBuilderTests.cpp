/*
 * RemoteCommandBuilderTests.cpp
 *
 *  Created on: Mar 27, 2017
 *      Author: Zarnowski
 */

#include "RemoteCommandBuilderTests.hpp"
#include "RemoteCommandBuilder.h"

static bool successScenarios() {

    RemoteCommandBuilder r1("PWD");
    if ("PWD\r" != r1.buildCommand()) {
        return false;
    }

    RemoteCommandBuilder r2("PWD");
    r2.addArgument(122);
    if ("PWD122\r" != r2.buildCommand()) {
        return false;
    }

    RemoteCommandBuilder r3("PWD");
    r3.addArgument(122);
    r3.addArgument(-2.2341);
    if ("PWD122,-2.234\r" != r3.buildCommand()) {
        return false;
    }

    RemoteCommandBuilder r4("PWD");
    r4.addArgument("test");
    if ("PWD\"test\"\r" != r4.buildCommand()) {
        return false;
    }

    RemoteCommandBuilder r5("PWD");
    r5.addArgument("test");
    r5.addArgument("jka");
    if ("PWD\"test\",\"jka\"\r" != r5.buildCommand()) {
        return false;
    }

    RemoteCommandBuilder r6("PWD");
    r6.startSequence();
    r6.addArgument(1);
    r6.addArgument(2);
    r6.endSequence();
    r6.startSequence();
    r6.addArgument(5);
    r6.addArgument(6);
    r6.endSequence();
    if ("PWD(1,2)(5,6)\r" != r6.buildCommand()) {
        return false;
    }

    RemoteCommandBuilder r7("PWD");
    r7.startSequence();
    r7.addArgument("test");
    r7.endSequence();
    r7.startSequence();
    r7.addArgument("jka");
    r7.endSequence();
    if ("PWD(\"test\")(\"jka\")\r" != r7.buildCommand()) {
        return false;
    }

    return true;
}

static bool failureScenarios() {
    try {
        RemoteCommandBuilder r("PWD");
        r.addArgument(122);
        r.addArgument("bad");
        return false;
    } catch (...) {
        //expected
    }

    try {
        RemoteCommandBuilder r("PWD");
        r.addArgument("bad");
        r.addArgument(122);
        return false;
    } catch (...) {
        //expected
    }

    try {
        RemoteCommandBuilder r("PWD");
        r.startSequence();
        r.addArgument("bad");
        r.addArgument(122);
        return false;
    } catch (...) {
        //expected
    }

    try {
        RemoteCommandBuilder r("PWD");
        r.startSequence();
        r.addArgument("good");
        r.endSequence();
        r.addArgument("bad");
        return false;
    } catch (...) {
        //expected
    }

    try {
        RemoteCommandBuilder r("PWD");
        r.startSequence();
        r.addArgument(2);
        r.endSequence();
        r.addArgument(6);
        return false;
    } catch (...) {
        //expected
    }

    try {
        RemoteCommandBuilder r("PWD");
        r.startSequence();
        r.addArgument(2);
        r.buildCommand();

        return false;
    } catch (...) {
        //expected
    }

    try {
        RemoteCommandBuilder r("PWD");
        r.startSequence();
        r.endSequence();

        return false;
    } catch (...) {
        //expected
    }

    return true;
}

bool testRemoteCommandBuilder() {
    bool testResult = true;

    testResult &= successScenarios();
    testResult &= failureScenarios();

    return true;
}
