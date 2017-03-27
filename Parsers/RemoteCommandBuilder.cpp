/*
 * RemoteCommandBuilder.cpp
 *
 *  Created on: Mar 27, 2017
 *      Author: Zarnowski
 */

#include "RemoteCommandBuilder.h"
#include <stdexcept>
#include <iomanip>
#include <sstream>

RemoteCommandBuilder::RemoteCommandBuilder(const string& cmd)
: outCmd(cmd), elementsType(UNKNOWN), isSequenceOpen(false), needComa(false), expectedNextSubsequence(false) {
    for (auto c = outCmd.begin() ; c < outCmd.end(); c++) {
        if (*c < 'A' || *c > 'Z') {
            throw invalid_argument("Only capital letters allowed!");
        }
    }
    if (outCmd.size() != 3) {
        throw invalid_argument("Cmd must be exactly 3 chars long!");
    }
}
void RemoteCommandBuilder::addArgument(int value) {
    addArgument((int64_t)value);
}
void RemoteCommandBuilder::addArgument(int64_t value) {
    if (elementsType == STRING) {
        throw invalid_argument("Digit expected, no mixed sequences are allowed!");
    }
    if (expectedNextSubsequence == true) {
        throw invalid_argument("Not allowed, call startSequence() first!");
    }
    elementsType = DIGIT;
    if (needComa == true) {
        outCmd += ',';
    }
    outCmd += to_string(value);
    needComa = true;
}

void RemoteCommandBuilder::addArgument(double value) {
    if (elementsType == STRING) {
        throw invalid_argument("Digit expected, no mixed sequences are allowed!");
    }
    if (expectedNextSubsequence == true) {
        throw invalid_argument("Not allowed, call startSequence() first!");
    }
    elementsType = DIGIT;
    if (needComa == true) {
        outCmd += ',';
    }
    stringstream str;
    str << fixed << setprecision( 3 ) << value;
    outCmd += str.str();
    needComa = true;
}

void RemoteCommandBuilder::addArgument(const string& value) {
    if (elementsType == DIGIT) {
        throw invalid_argument("String expected, no mixed sequences are allowed!");
    }
    if (expectedNextSubsequence == true) {
        throw invalid_argument("Not allowed, call startSequence() first!");
    }
    elementsType = STRING;
    if (needComa == true) {
        outCmd += ',';
    }
    outCmd += '"';
    outCmd += value;
    outCmd += '"';
    needComa = true;
}

void RemoteCommandBuilder::startSequence() {
    if (isSequenceOpen == true) {
        throw invalid_argument("No nested sequences allowed!");
    }
    isSequenceOpen = true;
    outCmd += "(";
    needComa = false;
    expectedNextSubsequence = false;
}

void RemoteCommandBuilder::endSequence() {
    if (isSequenceOpen == false) {
        throw invalid_argument("No sequence is open!");
    }
    if (needComa == false) {
        throw invalid_argument("At least one element in sequence is required!");
    }
    isSequenceOpen = false;
    outCmd += ")";
    needComa = false;
    expectedNextSubsequence = true;
}

string RemoteCommandBuilder::buildCommand() {
    if (isSequenceOpen == true) {
        throw invalid_argument("Last sequence is still open, call endSequence()!");
    }
    string tmp(outCmd);
    tmp += "\r";
    return tmp;
}
