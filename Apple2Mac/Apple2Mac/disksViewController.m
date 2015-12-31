//
//  disksViewController.m
//  Apple2Mac
//
//  Created by Jerome Vernet on 31/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import "disksViewController.h"
#import "common.h"

@implementation disksViewController



- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    
    
   
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    self.path = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"Disks"];
    self._disks=[[NSFileManager defaultManager] contentsOfDirectoryAtPath:self.path error:NULL];
    
    // Connect data
    self.disk1Picker.dataSource = self;
    self.disk1Picker.delegate = self;
    self.disk2Picker.dataSource = self;
    self.disk2Picker.delegate = self;
}

// The number of columns of data
- (int)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

// The number of rows of data
- (int)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
    return self._disks.count;
    //_pickerData.cout;
}

// The data to return for the row and component (column) that's being passed in
- (NSString*)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    return [self._disks objectAtIndex:row];
}

// Catpure the picker view selection
- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    // This method is triggered whenever the user makes a change to the picker selection.
    // The parameter named row and component represents what was selected.
    NSLog(@"Selected Row %d %@", row,(NSString*)[self._disks objectAtIndex:row]);
    disk6_eject(0);
    const char *errMsg = disk6_insert(0, [[self.path stringByAppendingPathComponent:[self._disks objectAtIndex:row]] UTF8String], YES);
}

- (IBAction)unwindToMainViewController:(UIStoryboardSegue*)sender
{ }

-(IBAction)goodbye:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:nil];
}

@end

