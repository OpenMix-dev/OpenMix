// Offers to move the app into /Applications on launch when it is running from
// its download location (Downloads, a mounted DMG, or an App Translocation
// path). Sparkle refuses to update from any of those, so without the move the
// user hits "OpenMix can't be updated if it's running from the location it was
// downloaded to". Compiled only on macOS.

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

static BOOL inApplicationsFolder(NSString* path) {
    NSArray<NSString*>* dirs =
        NSSearchPathForDirectoriesInDomains(NSApplicationDirectory, NSAllDomainsMask, YES);
    for (NSString* dir in dirs) {
        if ([path hasPrefix:[dir stringByAppendingString:@"/"]])
            return YES;
    }
    return [path hasPrefix:@"/Applications/"];
}

static void showMoveError(NSError* error) {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = @"Could not move OpenMix";
    alert.informativeText = [NSString
        stringWithFormat:@"%@\n\nYou can move OpenMix.app to the Applications folder manually.",
                         error.localizedDescription];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

extern "C" void openmix_offer_move_to_applications() {
    NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
    if (![bundlePath hasSuffix:@".app"])
        return; // bare dev binary, not an app bundle
    if (inApplicationsFolder(bundlePath))
        return;

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:@"OMMoveToApplicationsDeclined"])
        return;

    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Move OpenMix to the Applications folder?";
    alert.informativeText = @"OpenMix is running from its download location. "
                            @"Automatic updates only work from the Applications folder.";
    [alert addButtonWithTitle:@"Move to Applications"];
    [alert addButtonWithTitle:@"Don't Move"];
    if ([alert runModal] != NSAlertFirstButtonReturn) {
        [defaults setBool:YES forKey:@"OMMoveToApplicationsDeclined"];
        return;
    }

    NSFileManager* fm = [NSFileManager defaultManager];
    NSString* appsDir = @"/Applications";
    if (![fm isWritableFileAtPath:appsDir]) {
        appsDir = [NSHomeDirectory() stringByAppendingPathComponent:@"Applications"];
        [fm createDirectoryAtPath:appsDir withIntermediateDirectories:YES attributes:nil error:nil];
    }
    NSString* dest = [appsDir stringByAppendingPathComponent:[bundlePath lastPathComponent]];

    NSError* error = nil;
    if ([fm fileExistsAtPath:dest] && ![fm removeItemAtPath:dest error:&error]) {
        showMoveError(error);
        return;
    }
    if (![fm copyItemAtPath:bundlePath toPath:dest error:&error]) {
        showMoveError(error);
        return;
    }

    // strip quarantine so Gatekeeper does not translocate the copy on launch
    NSTask* xattr = [NSTask launchedTaskWithLaunchPath:@"/usr/bin/xattr"
                                             arguments:@[ @"-dr", @"com.apple.quarantine", dest ]];
    [xattr waitUntilExit];

    // tidy up the old copy when it is deletable (it is not on a mounted DMG or
    // under an App Translocation mount; the stale copy is harmless there)
    [fm trashItemAtURL:[NSURL fileURLWithPath:bundlePath] resultingItemURL:nil error:nil];

    // relaunch from the new location once this process has exited
    NSString* script =
        [NSString stringWithFormat:@"sleep 0.5; /usr/bin/open -n \"%@\"", dest];
    [NSTask launchedTaskWithLaunchPath:@"/bin/sh" arguments:@[ @"-c", script ]];
    exit(0);
}
