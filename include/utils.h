#ifndef _TTS_UTILS_H_
#define _TTS_UTILS_H_

#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<list>
#include<map>
#include<utility>
#include<string>
#include<unistd.h>
#include "stdlib.h"
#include "stdio.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "sys/mman.h"

int ttsLoadModel(char* ttsModelName, float** ttsModel);
void tts_free_data(void* data);


struct textStruct
{
    bool isCN = true;
    std::string text;
    textStruct(bool _isCN, const std::string& _text) {
        isCN = _isCN;
        text = _text;
    }
};

struct retStruct
{
    int32_t len = 0;
    int32_t startIdx = 0;
    int32_t endIdx = 0;
    int16_t* wavData = nullptr;
};


class TextSet {
public:
    std::list<textStruct> textList;

    TextSet(const char* path) {
        struct stat st;
        if (stat(path, &st) < 0 || st.st_size == 0) {
            std::cerr << "File [" << path << "] get stat fail.\n";
            exit(-1);
        }

        std::stringstream ss;
        ss << std::ifstream(path).rdbuf();
        auto str = preProcess(ss.str()); // 预处理：只保留汉字，英文，数字，替换部分符号

        int idxLast = 0, idx = 0;
        while (idx < str.length()) { // 分割中英文
            while (idx < str.length() && (((uint8_t)str[idx]) >= 128 || isdigit(str[idx]) || str[idx] == ' '))idx++;
            if (idx > idxLast) {
                textList.emplace_back(textStruct{ true, str.substr(idxLast, idx - idxLast) });
            }
            idxLast = idx;

            while (idx < str.length() && ((uint8_t)str[idx]) < 128 && !isdigit(str[idx]))idx++;
            if (idx > idxLast) {
                textList.emplace_back(textStruct{ false, str.substr(idxLast, idx - idxLast) });
            }
            idxLast = idx;
        }
    }

    size_t strReplace(std::string& src, const std::string& oldBlock, const std::string& newBlock) {
        if (oldBlock.empty())return 0;
        size_t cnt = 0, nextBeginIdx = 0, foundIdx;
        while ((foundIdx = src.find(oldBlock, nextBeginIdx)) != std::string::npos) {
            src.replace(foundIdx, oldBlock.length(), newBlock);
            nextBeginIdx = foundIdx + newBlock.length();
            cnt++;
        }
        return cnt;
    }

    std::string preProcess(std::string&& str) {
        const std::map<const int, const std::string> asciiMap = {
            {'+', "加"},
            {'-', "减"},
            {'*', "乘"},
            {'/', "除"},//斜杆?
            {'\\', "反斜杠"},
            {'_', "下划线"},
            {'~', "波浪号"},
            {'&', "与"},
            {'|', "或"},
            {'.', "点"},
            {'#', "井号"},
            {'$', "美元"},
            {'%', "百分比"},
            {'<', "小于"},
            {'=', "等于"},
            {'>', "大于"},
        };
        const std::pair<const std::string, const std::string> wordMap[]={
            {"fps", "帧"},
            {"℃", "摄氏度"},
            {"℉", "华氏度"},
            {"α", "阿尔法"},
            {"β", "贝塔"},
            {"γ", "伽马"},//还有很多
        };
        for(auto& item:wordMap)
            strReplace(str, item.first, item.second);

        int lastCodePoint = 'A', codePoint = 0, i = 0, len = str.length();
        std::vector<char> out;
        while (i < len) {
            if ((str[i] & 0x80) == 0) {
                codePoint = str[i];
                i++;
            } else if ((str[i] & 0xe0) == 0xc0) { // 110x'xxxx 10xx'xxxx
                codePoint = ((str[i] & 0x1f) << 6) | (str[i + 1] & 0x3f);
                i += 2;
            } else if ((str[i] & 0xf0) == 0xe0) { // 1110'xxxx 10xx'xxxx 10xx'xxxx
                codePoint = ((str[i] & 0x0f) << 12) | ((str[i + 1] & 0x3f) << 6) | (str[i + 2] & 0x3f);
                i += 3;
            } else if ((str[i] & 0xf8) == 0xf0) { // 1111'0xxx 10xx'xxxx 10xx'xxxx 10xx'xxxx
                codePoint = ((str[i] & 0x07) << 18) | ((str[i + 1] & 0x3f) << 12) | ((str[i + 2] & 0x3f) << 6) | (str[i + 3] & 0x3f);
                i += 4;
            } else { // 10xx'xxxx 等等非法
                codePoint = ' ';
                i++;
            }

            if (codePoint < 128) { // ascii
                if (('A' <= codePoint && codePoint <= 'Z') && ('A' <= lastCodePoint && lastCodePoint <= 'Z')) {
                    out.push_back(' ');
                    out.push_back(codePoint); // 如果是连着大写就插入空格隔开，单独发音，因为英文模型不会读全大写单词短语
                } else if (isalnum(codePoint)) {
                    out.push_back(codePoint);
                } else if (asciiMap.count(codePoint)) {
                    for (const char c : asciiMap.at(codePoint))
                        out.push_back(c);
                } else {
                    out.push_back(' ');
                }
            } else if (0x4e00 <= codePoint && codePoint <= 0x9fff) { // 中文区
                unsigned char byte1 = 0xe0 | ((codePoint >> 12) & 0x3f);
                unsigned char byte2 = 0x80 | ((codePoint >> 6) & 0x3f);
                unsigned char byte3 = 0x80 | (codePoint & 0x3f);

                out.push_back(byte1);
                out.push_back(byte2);
                out.push_back(byte3);
            } else { //忽略其他码点
                out.push_back(' ');
            }
            lastCodePoint = codePoint;
        }

        std::string ret(out.begin(), out.end());
        while (strReplace(ret, "  ", " "));
        return ret;
    }
};

class ModelData {
public:
    int size = 0;
    std::vector<float> dataBuff;
    ModelData(const char* path) {
        struct stat st;
        if (stat(path, &st) < 0 || st.st_size == 0) {
            std::cerr << "File [" << path << "] get stat fail.\n";
            exit(-1);
        }

        size = st.st_size;

        auto fp = fopen(path, "rb");
        if (!fp) {
            std::cerr << "File [" << path << "] open fail.\n";
            exit(-1);
        }
        dataBuff.reserve(size / 4 + (size % 4 == 0 ? 0 : 1));
        auto len = fread(dataBuff.data(), 1, size, fp);
        if (len != size) {
            std::cerr << "Read error.\n";
        }
        fclose(fp);
    }
    float* get() {
        return dataBuff.data();
    }
};


#endif
