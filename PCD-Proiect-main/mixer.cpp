// sudo apt install libsndfile1-dev
// g++-9 -o mixer mixer.cpp -lsfml-audio -lsndfile

#include <SFML/Audio.hpp>
#include <sndfile.h>
#include <vector>
#include <iostream>
#include <algorithm>

// Function to resample audio data
std::vector<sf::Int16> resample(const sf::Int16* samples, std::size_t sampleCount, unsigned int originalRate, unsigned int targetRate) {
    std::vector<sf::Int16> resampledSamples;
    double resampleRatio = static_cast<double>(originalRate) / targetRate;
    std::size_t newSampleCount = static_cast<std::size_t>(sampleCount / resampleRatio);
    resampledSamples.reserve(newSampleCount);

    for (std::size_t i = 0; i < newSampleCount; ++i) {
        std::size_t originalIndex = static_cast<std::size_t>(i * resampleRatio);
        resampledSamples.push_back(samples[originalIndex]);
    }

    return resampledSamples;
}

// Function to convert mono to stereo or vice versa
std::vector<sf::Int16> convertChannels(const sf::Int16* samples, std::size_t sampleCount, unsigned int originalChannels, unsigned int targetChannels) {
    std::vector<sf::Int16> convertedSamples;
    if (originalChannels == 1 && targetChannels == 2) {
        // Mono to Stereo
        convertedSamples.reserve(sampleCount * 2);
        for (std::size_t i = 0; i < sampleCount; ++i) {
            convertedSamples.push_back(samples[i]);
            convertedSamples.push_back(samples[i]);
        }
    } else if (originalChannels == 2 && targetChannels == 1) {
        // Stereo to Mono
        convertedSamples.reserve(sampleCount / 2);
        for (std::size_t i = 0; i < sampleCount; i += 2) {
            sf::Int16 monoSample = static_cast<sf::Int16>((samples[i] + samples[i + 1]) / 2);
            convertedSamples.push_back(monoSample);
        }
    }
    return convertedSamples;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <output file> <sound file 1> [<sound file 2> ... <sound file N>]" << std::endl;
        return -1;
    }

    std::string outputFilename = argv[1];
    std::vector<sf::SoundBuffer> buffers(argc - 2);

    // Load all sound files
    for (int i = 2; i < argc; ++i) {
        if (!buffers[i - 2].loadFromFile(argv[i])) {
            std::cerr << "Failed to load sound file: " << argv[i] << std::endl;
            return -1;
        }
    }

    // Determine target sample rate and channel count (based on the first sound file)
    unsigned int targetSampleRate = buffers[0].getSampleRate();
    unsigned int targetChannelCount = buffers[0].getChannelCount();

    // Resample and convert channel count if necessary
    std::vector<std::vector<sf::Int16>> processedSamples(buffers.size());
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        const sf::Int16* samples = buffers[i].getSamples();
        std::size_t sampleCount = buffers[i].getSampleCount();
        unsigned int sampleRate = buffers[i].getSampleRate();
        unsigned int channelCount = buffers[i].getChannelCount();

        if (sampleRate != targetSampleRate) {
            processedSamples[i] = resample(samples, sampleCount, sampleRate, targetSampleRate);
        } else {
            processedSamples[i] = std::vector<sf::Int16>(samples, samples + sampleCount);
        }

        if (channelCount != targetChannelCount) {
            processedSamples[i] = convertChannels(processedSamples[i].data(), processedSamples[i].size(), channelCount, targetChannelCount);
        }
    }

    // Determine the size of the output buffer
    std::size_t maxSampleCount = 0;
    for (const auto& samples : processedSamples) {
        if (samples.size() > maxSampleCount) {
            maxSampleCount = samples.size();
        }
    }

    std::vector<sf::Int16> mixedSamples(maxSampleCount, 0);

    // Mix the samples
    for (std::size_t i = 0; i < maxSampleCount; ++i) {
        sf::Int32 mixedSample = 0;
        for (const auto& samples : processedSamples) {
            if (i < samples.size()) {
                mixedSample += samples[i];
            }
        }

        // Ensure the mixed sample is within the valid range
        if (mixedSample > 32767) mixedSample = 32767;
        if (mixedSample < -32768) mixedSample = -32768;

        mixedSamples[i] = static_cast<sf::Int16>(mixedSample);
    }

    // Write the mixed sound file using libsndfile
    SF_INFO sfInfo;
    sfInfo.frames = mixedSamples.size() / targetChannelCount;
    sfInfo.samplerate = targetSampleRate;
    sfInfo.channels = targetChannelCount;
    sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outFile = sf_open(outputFilename.c_str(), SFM_WRITE, &sfInfo);
    if (!outFile) {
        std::cerr <<outputFilename.c_str()<< "\n";
        std::cerr << "Failed to create output sound file: " << sf_strerror(outFile) << std::endl;
        return -1;
    }

    sf_count_t count = sf_write_short(outFile, mixedSamples.data(), mixedSamples.size());
    if (count != static_cast<sf_count_t>(mixedSamples.size())) {
        std::cerr << "Failed to write samples to output sound file: " << sf_strerror(outFile) << std::endl;
        sf_close(outFile);
        return -1;
    }

    sf_close(outFile);

    std::cout << "Mixed sound saved as " << outputFilename << std::endl;
    return 0;
}
