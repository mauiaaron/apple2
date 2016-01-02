/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

// Modified sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#include "imageUtil.h"
#include "common.h"

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

demoImage *imgLoadImage(const char *filepathname, int flipVertical) {
    NSString *filepathString = [[NSString alloc] initWithUTF8String:filepathname];

#if TARGET_OS_IPHONE
    UIImage* imageClass = [[UIImage alloc] initWithContentsOfFile:filepathString];
#else
    NSImage *nsimage = [[NSImage alloc] initWithContentsOfFile: filepathString];
    NSBitmapImageRep *imageClass = [[NSBitmapImageRep alloc] initWithData:[nsimage TIFFRepresentation]];
    [nsimage release];
#endif

    CGImageRef cgImage = imageClass.CGImage;
    if (!cgImage)
    {
        [filepathString release];
        [imageClass release];
        return NULL;
    }

    demoImage *image = MALLOC(sizeof(demoImage));
    image->width = (GLuint)CGImageGetWidth(cgImage);
    image->height = (GLuint)CGImageGetHeight(cgImage);
    image->rowByteSize = image->width * 4;
    image->data = MALLOC(image->height * image->rowByteSize);
    image->format = GL_RGBA;
    image->type = GL_UNSIGNED_BYTE;

    CGContextRef context = CGBitmapContextCreate(image->data, image->width, image->height, 8, image->rowByteSize, CGImageGetColorSpace(cgImage), 0x0);
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    if (flipVertical) {
        CGContextTranslateCTM(context, 0.0, image->height);
        CGContextScaleCTM(context, 1.0, -1.0);
    }
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, image->width, image->height), cgImage);
    CGContextRelease(context);

    if (!image->data) {
        [filepathString release];
        [imageClass release];

        imgDestroyImage(image);
        return NULL;
    }

    [filepathString release];
    [imageClass release];

    return image;
}

void imgDestroyImage(demoImage* image) {
    FREE(image->data);
    FREE(image);
}

