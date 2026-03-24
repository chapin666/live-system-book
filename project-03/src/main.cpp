#include "live/live_player.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rtmp_url>" << std::endl;
        std::cerr << "Example: " << argv[0] << " rtmp://live.example.com/stream" << std::endl;
        return 1;
    }
    
    live::LivePlayer player;
    player.SetLiveMode(true);
    player.SetCatchUpStrategy(true);
    
    if (!player.PlayWithRetry(argv[1], 3)) {
        std::cerr << "直播连接失败" << std::endl;
        return 1;
    }
    
    return 0;
}