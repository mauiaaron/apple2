//
//  Apple2MacTests.m
//  Apple2MacTests
//
//  Created by Aaron Culliney on 6/21/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import <XCTest/XCTest.h>

@interface Apple2MacCPUTests : XCTestCase

@end

@implementation Apple2MacCPUTests

- (void)setUp
{
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown
{
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

extern int test_cpu(int, char **);

- (void)testCPU
{
    char *argv[] = {
        "-f",
        NULL
    };
    int argc = 0;
    for (char **p = &argv[0]; *p != NULL; p++) {
        ++argc;
    }
    int val = test_cpu(argc, argv);

    XCTAssertEqual(val, 0);
}

@end
