/*
 * InParser.cpp
 *
 *  Created on: Mar 24, 2017
 *      Author: Zarnowski
 */

#include "InParser.h"
#include <sstream>
#include <stdexcept>

bool RemoteCommand::operator==(const char* rhs) {
    return strcmp(cmd, (const char*) rhs) == 0;
}

bool RemoteCommand::operator==(const string& rhs) {
    return strcmp(cmd, (const char*) rhs.c_str()) == 0;
}

const RemoteCommandArgumentType RemoteCommand::getArgType() {
    return argType;
}

const Number RemoteCommand::argument(int index) {
  return numericValues[index];
}

const int64_t RemoteCommand::argumentAsInt(int index) {
  return numericValues[index].asInt64();
}

const uint64_t RemoteCommand::argumentAsUInt(int index) {
  return numericValues[index].asUInt64();
}

const double RemoteCommand::argumentAsDouble(int index) {
  return numericValues[index].asDouble();
}

shared_ptr<string> RemoteCommand::stringArgument(int index) {
  return stringValues[index];
}

const unsigned long RemoteCommand::argumentsCount() {
  //not only one of those structures can be non 0, thats why I can add them
  return numericValues.size() + stringValues.size() + numericSeries.size() + stringSeries.size();
}

shared_ptr<vector<Number> > RemoteCommand::getDigitSequence(int index) {
  return numericSeries[index];
}

shared_ptr<vector<shared_ptr<string> > > RemoteCommand::getStringSequence(int index) {
  return stringSeries[index];
}

InParser::InParser() {

}

InParser::~InParser() {
}

void InParser::parseCmd(istringstream& stream, shared_ptr<RemoteCommand> outCmd) {
    stream.read((char*)outCmd->cmd, 3);
    if (stream.eof()) {
        throw invalid_argument("Not enough data to form cmd");
    }
    outCmd->cmd[3] = 0;
    for(int t = 0; t < 3; t++) {
        const char c = outCmd->cmd[t];
        if (c < 'A' || c > 'Z') {
            throw invalid_argument("Invalid character in CMD, allowed only A-Z");
        }
    }
}

shared_ptr<string> InParser::handleSingleString(istringstream& stream) {
    char tmp;
    stream.read(&tmp, 1);

    if (tmp != '"' || stream.eof()) {
        throw invalid_argument("Invalid character expected '\"' on beginning of string argument");
    }
    shared_ptr<string> arg = make_shared<string>();
    while(true) {
        stream.read(&tmp, 1);
        if (stream.eof()) {
            throw invalid_argument("Unexpected end of data while parsing string");

        } else if (tmp < ' ' || tmp > '~') {
            throw invalid_argument("Invalid character accepted range is #32-#126 in string argument");

        } else if (tmp == '"') {
            break;
        }

        arg->push_back( tmp );
    }
    return arg;
}

void InParser::handleStringArgument(istringstream& stream, shared_ptr<RemoteCommand> outCmd) {
    outCmd->argType = RemoteCommandArgumentType_STRING;
    while(true) {
        shared_ptr<string> str = handleSingleString(stream);
        outCmd->stringValues.push_back(str);

        if (stream.eof()) {
            return;
        }
        char c;
        stream.read(&c, 1);
        if (stream.eof()) {
            return;
        }
        if (c != ',') {
            throw invalid_argument("Invalid character expected ',' in string sequence");
        }
        outCmd->argType = RemoteCommandArgumentType_STRING_SEQUENCE;
    }
}

Number InParser::handleSingleDigit(istringstream& stream) {
    stringstream tmpStream;
    bool isFloat = false;
    bool firstChar = true;

    while(true) {
        int peek = stream.peek();
        if (peek == ',' || peek == ')') {
            if (firstChar == true) {
                throw invalid_argument("Expected digit here!");
            }
            break;
        }
        char c;
        stream.read(&c, 1);
      
        if (stream.eof()) {
            if (firstChar == true) {
                throw invalid_argument("Expected digit here!");
            }
            break;
        }
      
        if (c == '.') {
            if (isFloat == true) {
                throw invalid_argument("To many '.'");
            }
            tmpStream << '.';
            isFloat = true;
            continue;
        }

        if (firstChar == true) {
          firstChar = false;   //it can be only at first char
          if (c == '-') {
            tmpStream << '-';
            continue;
          }
        }
        if (c >= '0' && c <= '9') {
            tmpStream << c;
            continue;
        }
      
        throw invalid_argument("Invalid character in digit.");
    }

    if (isFloat == true) {
        double tmp = strtod(tmpStream.str().c_str(), nullptr);
        return Number(tmp);

    } else {
        uint64_t tmp = strtoull(tmpStream.str().c_str(), nullptr, 10);
        return Number(tmp);
    }
}

void InParser::handleDigitArgument(istringstream& stream, shared_ptr<RemoteCommand> outCmd) {
    outCmd->argType = RemoteCommandArgumentType_DIGIT;
    while (true) {
        Number n = handleSingleDigit(stream);
        outCmd->numericValues.push_back(n);

        if (stream.eof()) {
            return;
        }
        char c;
        stream.read(&c, 1);
        if (c != ',') {
            throw invalid_argument("Invalid character expected ',' in digit sequence");
        }
        outCmd->argType = RemoteCommandArgumentType_DIGIT_SEQUENCE;
    }
}

void InParser::handleSequence(istringstream& stream, shared_ptr<RemoteCommand> outCmd) {
    char c;
    stream.read(&c, 1);  //read '('
    //discover if this is sequence of strings or digits?
    int peek = stream.peek();
    if (peek == EOF) {
        throw invalid_argument("Unexpected end of stream!");
    }

    bool isStrSeq = peek == '"';
    outCmd->argType = isStrSeq ? RemoteCommandArgumentType_STRING_MULTI_SEQUENCE : RemoteCommandArgumentType_DIGIT_MULTI_SEQUENCE;
    shared_ptr<vector<Number> > numberSeq = nullptr;
    shared_ptr<vector<shared_ptr<string> > > stringSeq = nullptr;

    while( true ) {
        if (isStrSeq == true) {
            shared_ptr<string> tmp = handleSingleString(stream);
            if (stringSeq == nullptr) {
                stringSeq = make_shared< vector<shared_ptr<string> > >();
            }
            stringSeq->push_back(tmp);

        } else {
            Number tmp = handleSingleDigit(stream);
            if (numberSeq == nullptr) {
                numberSeq = make_shared<vector<Number> >();
            }
            numberSeq->push_back(tmp);
        }

        stream.read(&c, 1);
        if (stream.eof() == true) {
            throw invalid_argument("Unexpected end of stream!");
        }
        if (c == ',') {
            //next element in sequence
            continue;
        }

        if (c == ')') {
            //end of subsequence
            if (isStrSeq) {
                outCmd->stringSeries.push_back(stringSeq);
                stringSeq = nullptr;
            } else {
                outCmd->numericSeries.push_back(numberSeq);
                numberSeq = nullptr;
            }

            stream.read(&c, 1);
            if (stream.eof() == true) {
                return; //end of sequence
            }

            if (c == '(') {
                //new sub sequence
                continue;
            }
        }

        throw invalid_argument("Illegal character in sequence!");
    }
}

shared_ptr<RemoteCommand> InParser::parse(shared_ptr<string> data) {
    shared_ptr<RemoteCommand> result = make_shared<RemoteCommand>();

    try {
        istringstream stream(*data);
        parseCmd(stream, result);
        int tmp = stream.peek();
        if (tmp == EOF) {
            result->argType = RemoteCommandArgumentType_NONE;

        } else {
            const char nextChar = static_cast<const char>(tmp);
            if (nextChar == '"') {
                //string
                handleStringArgument(stream, result);

            } else if (nextChar == '-' || (nextChar >= '0' && nextChar <= '9')) {
                //digit
                handleDigitArgument(stream, result);

            } else if (nextChar == '(') {
                //sequence
                handleSequence(stream, result);

            } else {
                //malformed
                throw invalid_argument("Malformed request");
            }
        }

    } catch (...) {
        return nullptr;
    }

    return result;
}
