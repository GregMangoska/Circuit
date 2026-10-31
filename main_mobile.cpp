#include "MobileGame.hpp"
#include <string>

#ifdef __ANDROID__
#include <SFML/System/NativeActivity.hpp>
#include <android/native_activity.h>
#endif

int main() {
    std::string savePath = "circuit.txt";
#ifdef __ANDROID__
    // App-private storage: survives restarts, needs no permissions.
    if (ANativeActivity* a = sf::getNativeActivity(); a && a->internalDataPath)
        savePath = std::string(a->internalDataPath) + "/circuit.txt";
#endif
    MobileGame game(savePath);
    game.run();
    return 0;
}
