// sudo apt install libsndfile1-dev
// g++-9 -o sequencer sequencer.cpp -lsfml-audio -lsndfile

#include <SFML/Audio.hpp>
#include <sndfile.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

// Helper function to parse the sequencing instructions from a text file
struct SequenceInstruction {
    int framesUntilPlayed;
    float pitch;
    float volume;
    int startSliceMs;
    int endSliceMs;
};

std::vector<SequenceInstruction> parseInstructions(const std::string& filename) {
    std::vector<SequenceInstruction> instructions;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        SequenceInstruction instruction;
        iss >> instruction.framesUntilPlayed >> instruction.pitch >> instruction.volume >> instruction.startSliceMs >> instruction.endSliceMs;
        instructions.push_back(instruction);
    }

    return instructions;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <sound file> <instructions file>" << std::endl;
        return -1;
    }

    std::string soundFilename = argv[1];
    std::string instructionsFilename = argv[2];

    // Load the original sound file
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(soundFilename)) {
        std::cerr << "Failed to load sound file." << std::endl;
        return -1;
    }

    // Get the samples and sample rate
    const sf::Int16* samples = buffer.getSamples();
    std::size_t sampleCount = buffer.getSampleCount();
    unsigned int sampleRate = buffer.getSampleRate();
    unsigned int channelCount = buffer.getChannelCount();

    // Parse the sequencing instructions
    std::vector<SequenceInstruction> instructions = parseInstructions(instructionsFilename);

    // Create a vector to hold the sequenced sound
    std::vector<sf::Int16> sequencedSamples;

    for (const auto& instruction : instructions) {
        // Convert milliseconds to samples
        int startSample = (instruction.startSliceMs * sampleRate) / 1000;
        int endSample = sampleCount - (instruction.endSliceMs * sampleRate) / 1000;

        // Ensure the slice boundaries are within the valid range
        startSample = std::max(0, startSample);
        endSample = std::min(static_cast<int>(sampleCount), endSample);

        // Apply pitch change (stretch/compress samples)
        std::vector<sf::Int16> pitchedSamples;
        for (int i = startSample; i < endSample; ++i) {
            int newIndex = static_cast<int>((i - startSample) / instruction.pitch);
            if (newIndex + startSample < endSample) {
                pitchedSamples.push_back(samples[newIndex + startSample]);
            }
        }

        // Apply volume change
        for (auto& sample : pitchedSamples) {
            sample = static_cast<sf::Int16>(sample * instruction.volume);
        }

        // Insert silence for frames until played
        int silenceFrames = instruction.framesUntilPlayed * channelCount;
        sequencedSamples.insert(sequencedSamples.end(), silenceFrames, 0);

        // Append the transformed samples to the sequenced sound
        sequencedSamples.insert(sequencedSamples.end(), pitchedSamples.begin(), pitchedSamples.end());
    }

    // Write the new sound file using libsndfile
    SF_INFO sfInfo;
    sfInfo.frames = sequencedSamples.size() / channelCount;
    sfInfo.samplerate = sampleRate;
    sfInfo.channels = channelCount;
    sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outFile = sf_open("sequenced.wav", SFM_WRITE, &sfInfo);
    if (!outFile) {
        std::cerr << "Failed to create output sound file." << std::endl;
        return -1;
    }

    sf_count_t count = sf_write_short(outFile, sequencedSamples.data(), sequencedSamples.size());
    if (count != sequencedSamples.size()) {
        std::cerr << "Failed to write samples to output sound file." << std::endl;
        sf_close(outFile);
        return -1;
    }

    sf_close(outFile);

    std::cout << "Sequenced sound saved as sequenced.wav" << std::endl;
    return 0;
}
