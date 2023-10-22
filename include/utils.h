#ifndef _TTS_UTILS_H_
#define _TTS_UTILS_H_

#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<list>
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
        std::stringstream ss;
        ss << std::ifstream(path).rdbuf();
        std::string str = ss.str();

        strReplace(str, "\r", " ");
        strReplace(str, "\n", " ");
        strReplace(str, "\t", " ");
        strReplace(str, "%", "百分比");
        strReplace(str, "，", " ");
        strReplace(str, "。", " ");
        while (strReplace(str, "  ", " "));

        int idxLast = 0, idx = 0;
        while (idx < str.length()) {
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

        filter();
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

    void filter() {
        for (auto it = textList.begin(); it != textList.end();) {
            std::string& text = (*it).text;
            if (text.back() == ' ')text.pop_back();

            if ((*it).isCN == false) {  // EN
                if (text.length() == 1) {
                    if (text[0] == '.') {
                        (*it).isCN = true;
                        (*it).text = "点";
                        it++;
                    } else if (isalpha(text[0])) {
                        it++;
                    } else {
                        it = textList.erase(it);
                    }
                } else {
                    it++;
                }
            } else { // CN
                it++;
            }
        }
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
