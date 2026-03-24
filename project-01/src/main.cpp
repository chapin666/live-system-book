#include "live/player.h"
#include <iostream>
#include <cstdlib>

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program << " <video_file>" << std::endl;
    std::cerr << "Controls:" << std::endl;
    std::cerr << "  SPACE - Pause/Resume" << std::endl;
    std::cerr << "  ESC   - Exit" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    const char* filename = argv[1];
    
    live::Player player;
    
    if (!player.Init(filename)) {
        std::cerr << "Failed to initialize player" << std::endl;
        return 1;
    }
    
    std::cout << "Starting playback..." << std::endl;
    std::cout << "Press SPACE to pause/resume, ESC to exit" << std::endl;
    
    player.Play();
    
    std::cout << "Playback finished." << std::endl;
    return 0;
}