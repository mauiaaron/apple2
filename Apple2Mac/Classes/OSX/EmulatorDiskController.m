/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2014, 2015 Aaron Culliney
 *
 */

#import "EmulatorDiskController.h"
#import "EmulatorPrefsController.h"
#import "common.h"

#define READONLY_CHOICE_INDEX 0
#define NO_DISK_INSERTED @"(No Disk Inserted)"
#define DSK_PROPERTIES @".dsk 143360 bytes"
#define NIB_PROPERTIES @".nib 232960 bytes"

#define kApple2DisksURL @"kApple2DisksURL"

@interface EmulatorDiskController ()

@property (assign) IBOutlet NSWindow *disksWindow;
@property (assign) IBOutlet NSTextField *diskInA;
@property (assign) IBOutlet NSTextField *diskInB;
@property (assign) IBOutlet NSTextField *diskAProperties;
@property (assign) IBOutlet NSTextField *diskBProperties;
@property (assign) IBOutlet NSMatrix *diskAProtection;
@property (assign) IBOutlet NSMatrix *diskBProtection;
@property (assign) IBOutlet NSButton *chooseDiskA;
@property (assign) IBOutlet NSButton *chooseDiskB;
@property (assign) IBOutlet NSButton *startupLoadDiskA;
@property (assign) IBOutlet NSButton *startupLoadDiskB;
@property (assign) IBOutlet NSButton *okButton;

- (void)loadPrefsForDomain:(const char *)domain;

@end

static EmulatorDiskController *diskInstance = nil;

static void prefsChangeCallback(const char *domain)
{
    (void)domain;
    assert(diskInstance);
    [diskInstance loadPrefsForDomain:domain];
}

@implementation EmulatorDiskController

- (void)awakeFromNib
{
#if CRASH_APP_ON_LOAD_BECAUSE_YAY_GJ_APPLE
    glGetError();
#endif
    assert(!diskInstance);
    diskInstance = self;

    prefs_registerListener(PREF_DOMAIN_VM, prefsChangeCallback);

    [self.diskInA setStringValue:NO_DISK_INSERTED];
    [self.diskAProperties setStringValue:@""];
    [self.diskInB setStringValue:NO_DISK_INSERTED];
    [self.diskBProperties setStringValue:@""];
    [self.chooseDiskA setKeyEquivalent:@"\r"];
    [self.chooseDiskA setBezelStyle:NSRoundedBezelStyle];
    [self.startupLoadDiskA setState:NSOffState];
    [self.startupLoadDiskB setState:NSOffState];
}

- (void)loadPrefsForDomain:(const char *)domain
{
    assert(strcmp(domain, PREF_DOMAIN_VM) == 0);
    (void)domain;

    {
        NSString *startupDiskA = nil;
        char *pathA = NULL;
        startupDiskA = prefs_copyStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_A, &pathA) ? [NSString stringWithUTF8String:pathA] : nil;
        FREE(pathA);

        bool bVal = false;
        BOOL readOnlyA = prefs_parseBoolValue(PREF_DOMAIN_VM, PREF_DISK_PATH_A_RO, &bVal) ? bVal : true;
        if (startupDiskA)
        {
            const char *path = [startupDiskA UTF8String];
            int fdA = -1;
            TEMP_FAILURE_RETRY(fdA = open(path, readOnlyA ? O_RDONLY : O_RDWR));
            const char *err = disk6_insert(fdA, 0, path, readOnlyA);
            if (fdA >= 0) {
                TEMP_FAILURE_RETRY(close(fdA));
            }
            if (!err)
            {
                [self.diskInA setStringValue:[[startupDiskA pathComponents] lastObject]];
                [self.startupLoadDiskA setState:NSOnState];
                [self.diskAProtection setState:(readOnlyA ? NSOnState : NSOffState) atRow:0 column:0];
                [self.diskAProtection setState:(!readOnlyA ? NSOnState : NSOffState) atRow:0 column:1];
            }
        }
    }
    
    {
        NSString *startupDiskB = nil;
        char *pathB = NULL;
        startupDiskB = prefs_copyStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_B, &pathB) ? [NSString stringWithUTF8String:pathB] : nil;
        FREE(pathB);

        bool bVal = false;
        BOOL readOnlyB = prefs_parseBoolValue(PREF_DOMAIN_VM, PREF_DISK_PATH_B_RO, &bVal) ? bVal : true;
        if (startupDiskB)
        {
            const char *path = [startupDiskB UTF8String];
            int fdB = -1;
            TEMP_FAILURE_RETRY(fdB = open(path, readOnlyB ? O_RDONLY : O_RDWR));
            const char *err = disk6_insert(fdB, 1, path, readOnlyB);
            if (fdB >= 0) {
                TEMP_FAILURE_RETRY(close(fdB));
            }
            if (!err)
            {
                [self.diskInB setStringValue:[[startupDiskB pathComponents] lastObject]];
                [self.startupLoadDiskB setState:NSOnState];
                [self.diskBProtection setState:(readOnlyB ? NSOnState : NSOffState) atRow:0 column:0];
                [self.diskBProtection setState:(!readOnlyB ? NSOnState : NSOffState) atRow:0 column:1];
            }
        }
    }
}

- (void)_savePrefs
{
    if (([self.startupLoadDiskA state] == NSOnState) && (disk6.disk[0].fd >= 0))
    {
        prefs_setStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_A, disk6.disk[0].file_name);
        NSButtonCell *readOnlyChoice = (NSButtonCell *)[[[self diskAProtection] cells] firstObject];
        prefs_setBoolValue(PREF_DOMAIN_VM, PREF_DISK_PATH_A_RO, ([readOnlyChoice state] == NSOnState));
    }
    else
    {
        prefs_setStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_A, "");
        prefs_setBoolValue(PREF_DOMAIN_VM, PREF_DISK_PATH_A_RO, true);
    }
    
    if (([self.startupLoadDiskB state] == NSOnState) && (disk6.disk[1].fd >= 0))
    {
        prefs_setStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_B, disk6.disk[1].file_name);
        NSButtonCell *readOnlyChoice = (NSButtonCell *)[[[self diskBProtection] cells] firstObject];
        prefs_setBoolValue(PREF_DOMAIN_VM, PREF_DISK_PATH_B_RO, ([readOnlyChoice state] == NSOnState) );
    }
    else
    {
        prefs_setStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_B, "");
        prefs_setBoolValue(PREF_DOMAIN_VM, PREF_DISK_PATH_B_RO, true);
    }

    prefs_sync(PREF_DOMAIN_VM);
    prefs_save();
}

- (void)_protectionChangedForDrive:(int)drive
{
    if (disk6.disk[drive].fd < 0)
    {
        return;
    }
    // HACK NOTE : dispatch so that state of outlet property is set properly
    dispatch_async(dispatch_get_main_queue(), ^{
        NSButtonCell *readOnlyChoice = (NSButtonCell *)[[(drive == 0 ? [self diskAProtection] : [self diskBProtection]) cells] firstObject];
        NSString *path = [NSString stringWithUTF8String:disk6.disk[drive].file_name];
        [self _insertDisketteInDrive:drive path:path type:[EmulatorDiskController extensionForPath:path] readOnly:([readOnlyChoice state] == NSOnState)];
        [self _savePrefs];
    });
}

- (IBAction)diskAProtectionChanged:(id)sender
{
    [self _protectionChangedForDrive:0];
}

- (IBAction)diskBProtectionChanged:(id)sender
{
    [self _protectionChangedForDrive:1];
}

- (BOOL)_insertDisketteInDrive:(int)drive path:(NSString *)path type:(NSString *)type readOnly:(BOOL)readOnly
{
    disk6_eject(drive);
    
    int fd = -1;
    TEMP_FAILURE_RETRY(fd = open([path UTF8String], readOnly ? O_RDONLY : O_RDWR));
    const char *errMsg = disk6_insert(fd, drive, [path UTF8String], readOnly);
    if (fd >= 0) {
        TEMP_FAILURE_RETRY(close(fd));
    }
    if (errMsg)
    {
        NSAlert *alert = [NSAlert alertWithError:[NSError errorWithDomain:[NSString stringWithUTF8String:errMsg] code:-1 userInfo:nil]];
        [alert beginSheetModalForWindow:[self disksWindow] completionHandler:nil];
        if (!drive)
        {
            [[self diskInA] setStringValue:NO_DISK_INSERTED];
            [[self diskAProperties] setStringValue:@""];
        }
        else
        {
            [[self diskInB] setStringValue:NO_DISK_INSERTED];
            [[self diskBProperties] setStringValue:@""];
        }
        return NO;
    }
    path = [NSString stringWithUTF8String:disk6.disk[drive].file_name];
    NSString *imageName = [[path pathComponents] lastObject];

    if (drive == 0)
    {
        [[self diskInA] setStringValue:imageName];

        bool isStartupDiskA = false;
        {
            char *pathA = NULL;
            if (prefs_copyStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_A, &pathA)) {
                isStartupDiskA = (strcmp(pathA, disk6.disk[drive].file_name) == 0);
            }
            FREE(pathA);
        }

        if (isStartupDiskA)
        {
            [self.startupLoadDiskA setState:NSOnState];
            //[self.diskAProtection setState:(readOnly ? NSOnState : NSOffState) atRow:0 column:0];
            //[self.diskAProtection setState:(!readOnly ? NSOnState : NSOffState) atRow:0 column:1];
        }
    }
    else
    {
        [[self diskInB] setStringValue:imageName];

        bool isStartupDiskB = false;
        {
            char *pathB = NULL;
            if (prefs_copyStringValue(PREF_DOMAIN_VM, PREF_DISK_PATH_B, &pathB)) {
                isStartupDiskB = (strcmp(pathB, disk6.disk[drive].file_name) == 0);
            }
            FREE(pathB);
        }

        if (isStartupDiskB)
        {
            [self.startupLoadDiskB setState:NSOnState];
            //[self.diskBProtection setState:(readOnly ? NSOnState : NSOffState) atRow:0 column:0];
            //[self.diskBProtection setState:(!readOnly ? NSOnState : NSOffState) atRow:0 column:1];
        }
    }
    
    if ([type isEqualToString:@"dsk"] || [type isEqualToString:@"do"] || [type isEqualToString:@"po"])
    {
        if (!drive)
        {
            [[self diskAProperties] setStringValue:DSK_PROPERTIES];
        }
        else
        {
            [[self diskBProperties] setStringValue:DSK_PROPERTIES];
        }
    }
    else
    {
        if (!drive)
        {
            [[self diskAProperties] setStringValue:NIB_PROPERTIES];
        }
        else
        {
            [[self diskBProperties] setStringValue:NIB_PROPERTIES];
        }
    }
    
    return YES;
}

+ (NSSet *)emulatorFileTypes
{
    static NSSet *set = nil;
    static dispatch_once_t onceToken = 0L;
    dispatch_once(&onceToken, ^{
        set = [[NSSet alloc] initWithObjects:@"dsk", @"nib", @"do", @"po", @"gz", nil];
    });
    return [[set retain] autorelease];
}

+ (NSString *)extensionForPath:(NSString *)path
{
    NSString *extension0 = [path pathExtension];
    NSString *extension1 = [[path stringByDeletingPathExtension] pathExtension];
    if ([extension0 isEqualToString:@"gz"])
    {
        extension0 = extension1;
    }
    return extension0;
}

+ (void)chooseDiskForWindow:(NSWindow *)window completionHandler:(DiskCompletionHandler)handler
{
    NSOpenPanel *openPanel = [[NSOpenPanel openPanel] retain];
    [openPanel setTitle:@"Choose a disk image"];
    NSURL *url = [[NSUserDefaults standardUserDefaults] URLForKey:kApple2DisksURL];
    if (!url)
    {
        url = [[NSBundle mainBundle] URLForResource:@"images" withExtension:nil];
    }
    [openPanel setDirectoryURL:url];
    [openPanel setShowsResizeIndicator:YES];
    [openPanel setShowsHiddenFiles:NO];
    [openPanel setCanChooseFiles:YES];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setCanCreateDirectories:NO];
    [openPanel setAllowsMultipleSelection:NO];
    
    // NOTE : Doesn't appear to be a way to specify ".dsk.gz" ... so we may inadvertently allow files of ".foo.gz" here
    NSSet *fileTypes = [EmulatorDiskController emulatorFileTypes];
    [openPanel setAllowedFileTypes:[fileTypes allObjects]];
    handler = Block_copy(handler);
    [openPanel beginSheetModalForWindow:window completionHandler:^(NSInteger result) {
        handler(openPanel, result);
        Block_release(handler);
        [openPanel autorelease];
    }];
}

- (void)_chooseDisk:(int)drive readOnly:(BOOL)readOnly
{
    [EmulatorDiskController chooseDiskForWindow:[self disksWindow] completionHandler:^(NSOpenPanel *openPanel, NSInteger result) {
        if (result == NSOKButton)
        {
            NSURL *selection = [[openPanel URLs] firstObject];
            NSString *path = [[selection path] stringByResolvingSymlinksInPath];
            NSString *extension = [EmulatorDiskController extensionForPath:path];
            NSSet *fileTypes = [EmulatorDiskController emulatorFileTypes];

            NSString *directory = [path stringByDeletingLastPathComponent];
            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            [defaults setURL:[NSURL URLWithString:directory] forKey:kApple2DisksURL];

            if (![fileTypes containsObject:extension])
            {
                NSAlert *alert = [NSAlert alertWithError:[NSError errorWithDomain:@"Disk image must have file extension of .dsk, .do, .po, or .nib only" code:-1 userInfo:nil]];
                [alert beginSheetModalForWindow:[self disksWindow] completionHandler:nil];
                return;
            }

            [self.chooseDiskA setKeyEquivalent:@""];
            [self.okButton setKeyEquivalent:@"\r"];

            [(drive == 0) ? self.startupLoadDiskA : self.startupLoadDiskB setState:NSOffState];
            [self _insertDisketteInDrive:drive path:path type:extension readOnly:readOnly];
        }
    }];
}

- (IBAction)chooseDriveA:(id)sender
{
    NSButtonCell *readOnlyChoice = (NSButtonCell *)[[[self diskAProtection] cells] firstObject];
    [self _chooseDisk:0 readOnly:([readOnlyChoice state] == NSOnState)];
}

- (IBAction)chooseDriveB:(id)sender
{
    NSButtonCell *readOnlyChoice = (NSButtonCell *)[[[self diskBProtection] cells] firstObject];
    [self _chooseDisk:1 readOnly:([readOnlyChoice state] == NSOnState)];
}

- (IBAction)startupDiskAChoiceChanged:(id)sender
{
    [self _savePrefs];
}

- (IBAction)startupDiskBChoiceChanged:(id)sender
{
    [self _savePrefs];
}

- (IBAction)disksOK:(id)sender
{
    [self.chooseDiskA setKeyEquivalent:@"\r"];
    [self.okButton setKeyEquivalent:@""];

    [[self disksWindow] close];
}

@end
