#import <Sparkle/Sparkle.h>

static SPUStandardUpdaterController* gUpdaterController = nil;

extern "C" void openmix_sparkle_init() {
    if (gUpdaterController)
        return;
    gUpdaterController =
        [[SPUStandardUpdaterController alloc] initWithStartingUpdater:YES
                                                     updaterDelegate:nil
                                                  userDriverDelegate:nil];
}

extern "C" void openmix_sparkle_check_with_ui() {
    if (gUpdaterController)
        [gUpdaterController checkForUpdates:nil];
}

extern "C" void openmix_sparkle_cleanup() {
    gUpdaterController = nil;
}
