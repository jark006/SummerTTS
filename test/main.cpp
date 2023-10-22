#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h> 

#include "Hanz2Piny.h"
#include "hanzi2phoneid.h"
#include "SynthesizerTrn.h"
#include "utils.h"

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::string;
using std::map;
using std::stringstream;

void saveToWavFile(const char* path, vector<retStruct>& data, int totalAudioLen);

int main(int argc, char** argv) {
    if (argc < 5) {
        cerr << "Need 4 parameters.\nUsage: ./tts_test txtPath CN_modelPath EN_modelPath outputPath\n\n";
        return -1;
    }

    TextSet textSet(argv[1]);
    ModelData cnModel(argv[2]), enModel(argv[3]);
    SynthesizerTrn synthesizerCN(cnModel.get(), cnModel.size);
    SynthesizerTrn synthesizerEN(enModel.get(), enModel.size);

    std::vector<retStruct> retList;
    int32_t lenSum = 0;
    for (auto& t : textSet.textList) {
        retStruct ret;
        if (t.isCN)ret.wavData = synthesizerCN.infer(t.text, 0, 1.0, ret.len);
        else ret.wavData = synthesizerEN.infer(t.text, 0, 1.0, ret.len);

        int32_t i = 0;
        while (i < ret.len && abs(ret.wavData[i]) < 10) i++;
        ret.startIdx = i;

        i = ret.len - 1;
        while (i > 0 && abs(ret.wavData[i]) < 10) i--;
        ret.endIdx = i;
        lenSum += ret.endIdx - ret.startIdx;

        retList.emplace_back(ret);
    }

    saveToWavFile(argv[4], retList, lenSum * sizeof(int16_t));

    return 0;
}


void saveToWavFile(const char* path, vector<retStruct>& data, int totalAudioLen) {
    char header[44];
    int byteRate = 16 * 16000 * 1 / 8;
    int totalDataLen = totalAudioLen + 36;
    int channels = 1;
    int longSampleRate = 16000;

    header[0] = 'R'; // RIFF/WAVE header
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    header[4] = (char)(totalDataLen & 0xff);
    header[5] = (char)((totalDataLen >> 8) & 0xff);
    header[6] = (char)((totalDataLen >> 16) & 0xff);
    header[7] = (char)((totalDataLen >> 24) & 0xff);
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f'; // 'fmt ' chunk
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    header[16] = 16; // 4 bytes: size of 'fmt ' chunk
    header[17] = 0;
    header[18] = 0;
    header[19] = 0;
    header[20] = 1; // format = 1
    header[21] = 0;
    header[22] = (char)channels;
    header[23] = 0;
    header[24] = (char)(longSampleRate & 0xff);
    header[25] = (char)((longSampleRate >> 8) & 0xff);
    header[26] = (char)((longSampleRate >> 16) & 0xff);
    header[27] = (char)((longSampleRate >> 24) & 0xff);
    header[28] = (char)(byteRate & 0xff);
    header[29] = (char)((byteRate >> 8) & 0xff);
    header[30] = (char)((byteRate >> 16) & 0xff);
    header[31] = (char)((byteRate >> 24) & 0xff);
    header[32] = (char)(1 * 16 / 8); // block align
    header[33] = 0;
    header[34] = 16; // bits per sample
    header[35] = 0;
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    header[40] = (char)(totalAudioLen & 0xff);
    header[41] = (char)((totalAudioLen >> 8) & 0xff);
    header[42] = (char)((totalAudioLen >> 16) & 0xff);
    header[43] = (char)((totalAudioLen >> 24) & 0xff);

    FILE* fpOut = fopen(path, "wb");
    if (!fpOut) {
        std::cerr << "File [" << path << "] write fail.\n";
        exit(-1);
    }

    fwrite(header, 1, 44, fpOut);
    for (const auto& r : data) {
        if (!r.wavData || r.startIdx == r.len)
            continue;
        fwrite(r.wavData + r.startIdx, 1, (r.endIdx - r.startIdx) * sizeof(int16_t), fpOut);
        free(r.wavData);
    }
    fclose(fpOut);
}
