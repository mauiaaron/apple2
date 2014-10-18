//
//  EmulatorDiskController.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 10/18/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import "EmulatorDiskController.h"

#define READONLY_CHOICE_INDEX 0
#define NO_DISK_INSERTED @"(No Disk Inserted)"
#define DSK_PROPERTIES @".dsk 143360 bytes"
#define NIB_PROPERTIES @".nib 232960 bytes"
#define GZ_EXTENSION @"gz"

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

@property (nonatomic, copy) NSString *diskAPath;
@property (nonatomic, copy) NSString *diskBPath;

@end

@implementation EmulatorDiskController

@synthesize diskAPath = _diskAPath;
@synthesize diskBPath = _diskBPath;

- (void)awakeFromNib
{
    [self.diskInA setStringValue:NO_DISK_INSERTED];
    [self.diskAProperties setStringValue:@""];
    [self.diskInB setStringValue:NO_DISK_INSERTED];
    [self.diskBProperties setStringValue:@""];
    [self.chooseDiskA setKeyEquivalent:@"\r"];
    [self.chooseDiskA setBezelStyle:NSRoundedBezelStyle];
}

- (void)dealloc
{
#warning TODO FIXME ... probably should exit emulator if this gets invoked ...
    self.diskAPath = nil;
    self.diskBPath = nil;
    [super dealloc];
}

- (IBAction)diskAProtectionChanged:(id)sender
{
    if ([[[self diskInA] stringValue] isEqualToString:NO_DISK_INSERTED])
    {
        return;
    }
    // HACK NOTE : dispatch so that state of outlet property is set properly
    dispatch_async(dispatch_get_main_queue(), ^{
        NSButtonCell *readOnlyChoice = [[[self diskAProtection] cells] firstObject];
        NSString *path = [self diskAPath];
        [self _insertDisketteInDrive:0 path:path type:[self _extensionForPath:path] readOnly:([readOnlyChoice state] == NSOnState)];
    });
}

- (IBAction)diskBProtectionChanged:(id)sender
{
    if ([[[self diskInB] stringValue] isEqualToString:NO_DISK_INSERTED])
    {
        return;
    }
    // HACK NOTE : dispatch so that state of outlet property is set properly
    dispatch_async(dispatch_get_main_queue(), ^{
        NSButtonCell *readOnlyChoice = [[[self diskBProtection] cells] firstObject];
        NSString *path = [self diskBPath];
        [self _insertDisketteInDrive:1 path:path type:[self _extensionForPath:path] readOnly:([readOnlyChoice state] == NSOnState)];
    });
}

- (BOOL)_insertDisketteInDrive:(int)drive path:(NSString *)path type:(NSString *)type readOnly:(BOOL)readOnly
{
    c_eject_6(drive);
    
    const char *errMsg = c_new_diskette_6(drive, [path UTF8String], readOnly);
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
    
    NSString *imageName = [[path pathComponents] lastObject];
    
    if (!drive)
    {
        self.diskAPath = path;
        [[self diskInA] setStringValue:imageName];
    }
    else
    {
        self.diskBPath = path;
        [[self diskInB] setStringValue:imageName];
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

- (NSString *)_extensionForPath:(NSString *)path
{
    NSString *extension0 = [path pathExtension];
    NSString *extension1 = [[path stringByDeletingPathExtension] pathExtension];
    if ([extension0 isEqualToString:GZ_EXTENSION])
    {
        extension0 = extension1;
    }
    return extension0;
}

- (void)_chooseDisk:(int)drive readOnly:(BOOL)readOnly
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setTitle:@"Choose a disk image"];
    NSURL *url = [[NSUserDefaults standardUserDefaults] URLForKey:kApple2DisksURL];
    if (!url)
    {
        url = [[NSBundle mainBundle] URLForResource:@"blank" withExtension:@"dsk.gz"];
        url = [url URLByDeletingLastPathComponent];
    }
    [[NSUserDefaults standardUserDefaults] setURL:url forKey:kApple2DisksURL];
    [openPanel setDirectoryURL:url];
    [openPanel setShowsResizeIndicator:YES];
    [openPanel setShowsHiddenFiles:NO];
    [openPanel setCanChooseFiles:YES];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setCanCreateDirectories:NO];
    [openPanel setAllowsMultipleSelection:NO];
    
    // NOTE : Doesn't appear to be a way to specify ".dsk.gz" ... so we may inadvertently allow files of ".foo.gz" here
    NSSet *fileTypes = [NSSet setWithObjects:@"dsk", @"nib", @"do", @"po", GZ_EXTENSION, nil];
    [openPanel setAllowedFileTypes:[fileTypes allObjects]];
    [openPanel beginSheetModalForWindow:[self disksWindow] completionHandler:^(NSInteger result) {
        if (result == NSOKButton)
        {
            NSURL *selection = [[openPanel URLs] firstObject];
            NSString *path = [[selection path] stringByResolvingSymlinksInPath];
            NSString *extension = [self _extensionForPath:path];
            
            if (![fileTypes containsObject:extension])
            {
                NSAlert *alert = [NSAlert alertWithError:[NSError errorWithDomain:@"Disk image must have file extension of .dsk, .do, .po, or .nib only" code:-1 userInfo:nil]];
                [alert beginSheetModalForWindow:[self disksWindow] completionHandler:nil];
                return;
            }
            
            [self _insertDisketteInDrive:drive path:path type:extension readOnly:readOnly];
        }
    }];
}

- (IBAction)chooseDriveA:(id)sender
{
    NSButtonCell *readOnlyChoice = [[[self diskAProtection] cells] firstObject];
    [self _chooseDisk:0 readOnly:([readOnlyChoice state] == NSOnState)];
}

- (IBAction)chooseDriveB:(id)sender
{
    NSButtonCell *readOnlyChoice = [[[self diskBProtection] cells] firstObject];
    [self _chooseDisk:1 readOnly:([readOnlyChoice state] == NSOnState)];
}

@end
