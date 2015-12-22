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

@end

@implementation EmulatorDiskController

- (void)awakeFromNib
{
#if CRASH_APP_ON_LOAD_BECAUSE_YAY_GJ_APPLE
    glGetError();
#endif
    
    [self.diskInA setStringValue:NO_DISK_INSERTED];
    [self.diskAProperties setStringValue:@""];
    [self.diskInB setStringValue:NO_DISK_INSERTED];
    [self.diskBProperties setStringValue:@""];
    [self.chooseDiskA setKeyEquivalent:@"\r"];
    [self.chooseDiskA setBezelStyle:NSRoundedBezelStyle];
    [self.startupLoadDiskA setState:NSOffState];
    [self.startupLoadDiskB setState:NSOffState];
    
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    NSString *startupDiskA = [defaults stringForKey:kApple2PrefStartupDiskA];
    BOOL readOnlyA = [defaults boolForKey:kApple2PrefStartupDiskAProtected];
    if (startupDiskA)
    {
        const char *err = disk6_insert(0, [[NSString stringWithFormat:@"%@.gz", startupDiskA] UTF8String], readOnlyA);
        if (!err)
        {
            [self.diskInA setStringValue:[[startupDiskA pathComponents] lastObject]];
            [self.startupLoadDiskA setState:NSOnState];
            [self.diskAProtection setState:(readOnlyA ? NSOnState : NSOffState) atRow:0 column:0];
            [self.diskAProtection setState:(!readOnlyA ? NSOnState : NSOffState) atRow:0 column:1];
        }
    }
    
    NSString *startupDiskB = [defaults stringForKey:kApple2PrefStartupDiskB];
    BOOL readOnlyB = [defaults boolForKey:kApple2PrefStartupDiskBProtected];
    if (startupDiskB)
    {
        const char *err = disk6_insert(1, [[NSString stringWithFormat:@"%@.gz", startupDiskB] UTF8String], readOnlyB);
        if (!err)
        {
            [self.diskInB setStringValue:[[startupDiskB pathComponents] lastObject]];
            [self.startupLoadDiskB setState:NSOnState];
            [self.diskBProtection setState:(readOnlyB ? NSOnState : NSOffState) atRow:0 column:0];
            [self.diskBProtection setState:(!readOnlyB ? NSOnState : NSOffState) atRow:0 column:1];
        }
    }
}

- (void)_savePrefs
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    [defaults removeObjectForKey:kApple2PrefStartupDiskA];
    [defaults removeObjectForKey:kApple2PrefStartupDiskB];
    [defaults removeObjectForKey:kApple2PrefStartupDiskAProtected];
    [defaults removeObjectForKey:kApple2PrefStartupDiskBProtected];
    
    if ([self.startupLoadDiskA state] == NSOnState)
    {
        if (disk6.disk[0].fd >= 0)
        {
            NSString *diskA = [NSString stringWithUTF8String:disk6.disk[0].file_name];
            [defaults setObject:diskA forKey:kApple2PrefStartupDiskA];
            NSButtonCell *readOnlyChoice = (NSButtonCell *)[[[self diskAProtection] cells] firstObject];
            [defaults setBool:([readOnlyChoice state] == NSOnState) forKey:kApple2PrefStartupDiskAProtected];
        }
    }
    
    if ([self.startupLoadDiskB state] == NSOnState)
    {
        if (disk6.disk[1].fd >= 0)
        {
            NSString *diskB = [NSString stringWithUTF8String:disk6.disk[1].file_name];
            [defaults setObject:diskB forKey:kApple2PrefStartupDiskB];
            NSButtonCell *readOnlyChoice = (NSButtonCell *)[[[self diskBProtection] cells] firstObject];
            [defaults setBool:([readOnlyChoice state] == NSOnState) forKey:kApple2PrefStartupDiskBProtected];
        }
    }
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
    
    const char *errMsg = disk6_insert(drive, [path UTF8String], readOnly);
    if (errMsg)
    {
        path = [NSString stringWithFormat:@"%@.gz", path];
        errMsg = disk6_insert(drive, [path UTF8String], readOnly);
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
    }
    path = [NSString stringWithUTF8String:disk6.disk[drive].file_name];
    NSString *imageName = [[path pathComponents] lastObject];
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    if (drive == 0)
    {
        [[self diskInA] setStringValue:imageName];
        if ([[defaults stringForKey:kApple2PrefStartupDiskA] isEqualToString:path])
        {
            [self.startupLoadDiskA setState:NSOnState];
            //[self.diskAProtection setState:(readOnly ? NSOnState : NSOffState) atRow:0 column:0];
            //[self.diskAProtection setState:(!readOnly ? NSOnState : NSOffState) atRow:0 column:1];
        }
    }
    else
    {
        [[self diskInB] setStringValue:imageName];
        if ([[defaults stringForKey:kApple2PrefStartupDiskB] isEqualToString:path])
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
    [[self disksWindow] close];
}

@end
