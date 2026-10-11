// GameCenter.mm - achievements on iPhone and iPad, through Apple's Game Center
// (GameKit). Called from src/PlatformServices.cpp. Built with ARC (CMakeLists.txt).
//
// Sign-in: iOS signs the player in automatically if they use Game Center; if it
// needs them to sign in, it hands over a screen, which is shown over the game.
// Unlocks made before sign-in finishes wait and are sent once it does.
#import <GameKit/GameKit.h>
#import <UIKit/UIKit.h>

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace gamecenter {

namespace {
bool gSignedIn = false;
std::vector<std::string> gWaiting; // unlocks from before sign-in finished

void report(const std::string& ident) {
    GKAchievement* a = [[GKAchievement alloc] initWithIdentifier:[NSString stringWithUTF8String:ident.c_str()]];
    a.percentComplete = 100.0;
    a.showsCompletionBanner = YES; // Game Center's own "achievement unlocked" banner
    [GKAchievement reportAchievements:@[ a ]
                withCompletionHandler:^(NSError* error) {
                  if (error)
                      SDL_Log("Game Center: couldn't report '%s': %s (is it set up in App Store Connect?)",
                              ident.c_str(), error.localizedDescription.UTF8String);
                }];
}
} // namespace

void init(SDL_Window* window) {
    GKLocalPlayer* player = GKLocalPlayer.local;
    player.authenticateHandler = ^(UIViewController* signIn, NSError* error) {
      if (signIn) {
          // Show Game Center's sign-in over the game.
          SDL_PropertiesID props = SDL_GetWindowProperties(window);
          UIWindow* uiWindow =
              (__bridge UIWindow*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
          UIViewController* root = uiWindow.rootViewController;
          if (root) [root presentViewController:signIn animated:YES completion:nil];
          return;
      }
      gSignedIn = player.isAuthenticated;
      if (gSignedIn) {
          SDL_Log("Game Center: signed in, achievements go to Game Center");
          for (const auto& id : gWaiting) report(id);
          gWaiting.clear();
      } else if (error) {
          SDL_Log("Game Center: not signed in (%s) - achievements stay in the game",
                  error.localizedDescription.UTF8String);
      }
    };
}

void unlock(const char* id) {
    if (gSignedIn) report(id);
    else gWaiting.push_back(id);
}

bool signedIn() { return gSignedIn; }

} // namespace gamecenter
