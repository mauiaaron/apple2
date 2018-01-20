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

#import "EmulatorJoystickController.h"
#import "EmulatorWindowController.h"
#import "common.h"

#import <DDHidLib/DDHidJoystick.h>

@interface EmulatorJoystickController()
@property (nonatomic, retain) NSDictionary *allJoysticks;
+ (EmulatorJoystickController *)sharedInstance;
- (void)resetJoysticks;
@end

@implementation EmulatorJoystickController

@synthesize allJoysticks = _allJoysticks;

+ (EmulatorJoystickController *)sharedInstance
{
    static dispatch_once_t onceToken = 0L;
    static EmulatorJoystickController *joystickController = nil;
    dispatch_once(&onceToken, ^{
        joystickController = [[EmulatorJoystickController alloc] init];
    });
    return joystickController;
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(connectivityPoll:) name:(NSString *)kDrawTimerNotification object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    self.allJoysticks = nil;
    [super dealloc];
}

- (void)resetJoysticks
{
    for (DDHidJoystick *joystick in [self.allJoysticks allValues])
    {
        @try {
            [joystick setDelegate:nil];
            [joystick stopListening];
        } @catch (NSException *e) {
            // hot-plugging joysticks can cause glitches
            NSLog(@"Joystick device library raised exception on close : %@", e);
        }
    }
    NSArray *joysticks = [DDHidJoystick allJoysticks];
    NSMutableDictionary *allJoysticks = [NSMutableDictionary dictionary];
    for (DDHidJoystick *joystick in joysticks) {
        NSString *key =[NSString stringWithFormat:@"%@-%@-%@", [joystick manufacturer], [joystick serialNumber], [joystick transport]];
        [allJoysticks setObject:joystick forKey:key];
    }
    self.allJoysticks = allJoysticks;
    dispatch_async(dispatch_get_main_queue(), ^ {
        for (DDHidJoystick *joystick in [self.allJoysticks allValues])
        {
            NSLog(@"found joystick : '%@' '%@' '%@' %@", [joystick manufacturer], [joystick serialNumber], [joystick transport], joystick);
            @try {
                [joystick setDelegate:self];
                [joystick startListening];
            } @catch (NSException *e) {
                // hot-plugging joystick can cause glitches
                NSLog(@"Joystick device library raised exception on start : %@", e);
            }
        }
    });
}

#pragma mark -
#pragma mark Joystick connectivity polling

- (void)connectivityPoll:(NSNotification *)notification
{
    static unsigned int counter = 0;
    counter = (counter+1) % 60;
    if (counter == 0)
    {
        NSArray *joysticks = [DDHidJoystick allJoysticks];
        BOOL changed = ([joysticks count] != [self.allJoysticks count]);
        for (DDHidJoystick *joystick in joysticks)
        {
            NSString *key =[NSString stringWithFormat:@"%@-%@-%@", [joystick manufacturer], [joystick serialNumber], [joystick transport]];
            if (![self.allJoysticks objectForKey:key])
            {
                changed = YES;
                break;
            }
        }
        if (changed)
        {
            [self resetJoysticks];
        }
    }
#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        c_keys_handle_input(-1, 0, 0);
    }
#endif
}

#pragma mark -
#pragma mark NSObject(DDHidJoystickDelegate)

#define DDHID_JOYSTICK_NORMALIZER ((float)JOY_RANGE/(DDHID_JOYSTICK_VALUE_MAX*2))

- (void)ddhidJoystick:(DDHidJoystick *)joystick stick:(unsigned int)stick xChanged:(int)value
{
#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD) {
        return;
    }
#endif
    
    int x = (int)((value+DDHID_JOYSTICK_VALUE_MAX) * DDHID_JOYSTICK_NORMALIZER);
    if (x < 0) {
        x = 0;
    }
    if (x >= JOY_RANGE) {
        x = JOY_RANGE-1;
    }
    joydriver_setAxisValue((uint8_t)x, joydriver_getAxisY());
}

- (void)ddhidJoystick:(DDHidJoystick *)joystick stick:(unsigned int)stick yChanged:(int)value
{
#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD) {
        return;
    }
#endif
    
    int y = (int)((value+DDHID_JOYSTICK_VALUE_MAX) * DDHID_JOYSTICK_NORMALIZER);
    if (y < 0) {
        y = 0;
    }
    if (y >= JOY_RANGE) {
        y = JOY_RANGE-1;
    }
    joydriver_setAxisValue(joydriver_getAxisX(), (uint8_t)y);
}

- (void)ddhidJoystick:(DDHidJoystick *)joystick stick:(unsigned int)stick otherAxis:(unsigned)otherAxis valueChanged:(int)value
{
    // NOOP ...
}

- (void)ddhidJoystick:(DDHidJoystick *)joystick stick:(unsigned int)stick povNumber:(unsigned)povNumber valueChanged:(int)value
{
    // NOOP ...
}

- (void)ddhidJoystick:(DDHidJoystick *)joystick buttonDown:(unsigned int)buttonNumber
{
#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD) {
        return;
    }
#endif
    
    // sample buttons only if apple keys aren't pressed. keys get set to 0xff, and js buttons are set to 0x80.
    if ((buttonNumber == 0x01) && !(run_args.joy_button0 & 0x7f)) {
        run_args.joy_button0 = 0x80;
    }
    if ((buttonNumber == 0x02) && !(run_args.joy_button1 & 0x7f)) {
        run_args.joy_button1 = 0x80;
    }
}

- (void)ddhidJoystick:(DDHidJoystick *)joystick buttonUp:(unsigned int)buttonNumber
{
#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD) {
        return;
    }
#endif

    // sample buttons only if apple keys aren't pressed. keys get set to 0xff, and js buttons are set to 0x80.
    if ((buttonNumber == 0x01) && !(run_args.joy_button0 & 0x7f)) {
        run_args.joy_button0 = 0x0;
    }
    if ((buttonNumber == 0x02) && !(run_args.joy_button1 & 0x7f)) {
        run_args.joy_button1 = 0x0;
    }
}


@end
