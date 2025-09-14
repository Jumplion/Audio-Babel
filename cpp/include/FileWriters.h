#ifndef FILE_WRITERS_H
#define FILE_WRITERS_H

#include <boost/multiprecision/cpp_int.hpp>
#include <string>

#include "AudioIndex.h"

namespace AudioBabel {

class FileWriters {
   public:
    static void exportAudioDataToWav(const AudioIndex::AudioData& audioData, const std::string& path);

    static void writeIndexToFile(const boost::multiprecision::cpp_int& index,
                                 const std::string&                    outDir   = std::string(),
                                 const std::string&                    filename = std::string());
};

} // namespace AudioBabel

#endif // FILE_WRITERS_H
