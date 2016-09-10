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

#include "glhudmodel.h"
#include "glnode.h"

// . . .
// . x .
// . . .
typedef struct EightPatchArgs_s {
    GLModel *parent;
    unsigned int pixelSize;
    unsigned int glyphScale;
    unsigned int fb_h;
    unsigned int fb_w;
    unsigned int srcIdx;
    unsigned int dstIdx;
} EightPatchArgs_s;

#ifndef NDEBUG
#   define SET_TEX_PIXEL(OFF) \
    do { \
        assert((OFF) >= 0 && (OFF) < lastPoint); \
        *((PIXEL_TYPE*)(texPixels + (OFF))) |= SEMI_OPAQUE; \
    } while (0);
#else
#   define SET_TEX_PIXEL(OFF) \
    do { \
        *((PIXEL_TYPE*)(texPixels + (OFF))) |= SEMI_OPAQUE; \
    } while (0);
#endif

// Generates a semi-opaque halo effect around each glyph
static void _eightpatch_opaquePixelHaloFilter(const EightPatchArgs_s args) {

#if USE_RGBA4444
#   define SEMI_OPAQUE (0x0C << SHIFT_A)
#else
#   define SEMI_OPAQUE (0xC0 << SHIFT_A)
#endif

    const unsigned int pixelSize = args.pixelSize;
    const unsigned int glyphScale = args.glyphScale;
    const unsigned int fb_w = args.fb_w;
    const unsigned int fb_h = args.fb_h;
    const unsigned int srcIdx = args.srcIdx;
    const unsigned int srcCol = (srcIdx % fb_w);
    const unsigned int lastCol = (fb_w-1);
    const unsigned int dstPointStride = pixelSize * glyphScale;
    const unsigned int dstRowStride = fb_w * dstPointStride;
    const unsigned int texRowStride = ((fb_w * glyphScale) * (FONT_GLYPH_SCALE_Y * glyphScale) * pixelSize);
    const unsigned int lastPoint    = ((fb_w * glyphScale) * (fb_h * glyphScale) * pixelSize);

    uint8_t *texPixels = args.parent->texPixels;
    const int dstIdx0 = (int)args.dstIdx;
    const int dstPre0 = dstIdx0 - texRowStride; // negative is okay
    const int dstPost0 = dstIdx0 + texRowStride;

    // scale glyph data 1x, 2x, ...

    // north pixels
    if (dstPre0 >= 0) {
        int dstPre = dstPre0;
        for (unsigned int k=0; k<glyphScale; k++, dstPre+=dstRowStride) {
            for (unsigned int l=0; l<glyphScale; l++, dstPre+=pixelSize) {
                if (srcCol != 0) {
                    SET_TEX_PIXEL(dstPre - dstPointStride);
                }
                SET_TEX_PIXEL(dstPre);
                if (srcCol < lastCol) {
                    SET_TEX_PIXEL(dstPre + dstPointStride);
                }
            }
            dstPre -= dstPointStride;
        }
    }

    // west pixel
    if (srcCol != 0) {
        int dstIdx = dstIdx0;
        for (unsigned int k=0; k<glyphScale; k++, dstIdx+=dstRowStride) {
            for (unsigned int l=0; l<glyphScale; l++, dstIdx+=pixelSize) {
                SET_TEX_PIXEL(dstIdx - dstPointStride);
            }
            dstIdx -= dstPointStride;
        }
    }

    // east pixel
    if (srcCol < lastCol) {
        int dstIdx = dstIdx0;
        for (unsigned int k=0; k<glyphScale; k++, dstIdx+=dstRowStride) {
            for (unsigned int l=0; l<glyphScale; l++, dstIdx+=pixelSize) {
                SET_TEX_PIXEL(dstIdx + dstPointStride);
            }
            dstIdx -= dstPointStride;
        }
    }

    // south pixels
    if (dstPost0 < lastPoint) {
        int dstPost = dstPost0;
        for (unsigned int k=0; k<glyphScale; k++, dstPost+=dstRowStride) {
            for (unsigned int l=0; l<glyphScale; l++, dstPost+=pixelSize) {
                if (srcCol != 0) {
                    SET_TEX_PIXEL(dstPost - dstPointStride);
                }
                SET_TEX_PIXEL(dstPost);
                if (srcCol < lastCol) {
                    SET_TEX_PIXEL(dstPost + dstPointStride);
                }
            }
            dstPost -= dstPointStride;
        }
    }
}

// ----------------------------------------------------------------------------

void *glhud_createDefault(void) {
    return glhud_createCustom(sizeof(GLModelHUDElement));
}

void *glhud_createCustom(unsigned int sizeofModel) {
    assert(sizeof(GLModelHUDElement) <= sizeofModel);
    GLModelHUDElement *hudElement = (GLModelHUDElement *)CALLOC(sizeofModel, 1);
    if (hudElement) {
        hudElement->glyphMultiplier = 1;
        hudElement->colorScheme = RED_ON_BLACK;
    }
    return hudElement;
}

void glhud_setupDefault(GLModel *parent) {

    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    assert(hudElement->glyphMultiplier > 0);

    char *submenu = (char *)(hudElement->tpl);
    const unsigned int cols = hudElement->tplWidth;
    const unsigned int rows = hudElement->tplHeight;
    uint8_t *fb = hudElement->pixels;

    // render template into indexed fb
    interface_plotMessage(fb, hudElement->colorScheme, submenu, cols, rows);

    // generate OpenGL texture/color from indexed color
    const unsigned int fb_w = hudElement->pixWidth;
    const unsigned int fb_h = hudElement->pixHeight;
    const unsigned int pixelSize = sizeof(PIXEL_TYPE);
    const unsigned int glyphScale = hudElement->glyphMultiplier;
    const unsigned int dstPointStride = pixelSize * glyphScale;
    const unsigned int dstRowStride = fb_w * dstPointStride;
    const unsigned int texSubRowStride = dstRowStride * (glyphScale-1);

    do {
        unsigned int srcIdx = 0;
        unsigned int texIdx = 0;
#ifndef NDEBUG
        const unsigned int srcLastPoint = fb_w * fb_h;
        const unsigned int texLastPoint = ((fb_w * glyphScale) * (fb_h * glyphScale) * pixelSize);
#endif
        for (unsigned int i=0; i<fb_h; i++, texIdx+=texSubRowStride) {
            for (unsigned int j=0; j<fb_w; j++, srcIdx++, texIdx+=dstPointStride) {
                uint8_t value = *(fb + srcIdx);
                PIXEL_TYPE rgba = (((PIXEL_TYPE)(colormap[value].red)   << SHIFT_R) |
                                   ((PIXEL_TYPE)(colormap[value].green) << SHIFT_G) |
                                   ((PIXEL_TYPE)(colormap[value].blue)  << SHIFT_B));
                if (rgba == 0 && hudElement->blackIsTransparent) {
                    // black remains transparent
                } else {
                    rgba |=        ((PIXEL_TYPE)MAX_SATURATION          << SHIFT_A);
                }

                // scale glyph data 1x, 2x, ...
                unsigned int dstIdx = texIdx;
                for (unsigned int k=0; k<glyphScale; k++, dstIdx+=dstRowStride) {
                    for (unsigned int l=0; l<glyphScale; l++, dstIdx+=pixelSize) {
                        *( (PIXEL_TYPE *)(parent->texPixels + dstIdx) ) = rgba;
                    }
                    dstIdx -= dstPointStride;
                }
            }
        }
#ifndef NDEBUG
        assert(srcIdx == srcLastPoint);
        assert(texIdx == texLastPoint);
#endif
    } while (0);

    if (hudElement->opaquePixelHalo) {
        unsigned int srcIdx = 0;
        unsigned int texIdx = 0;
#ifndef NDEBUG
        const unsigned int srcLastPoint = fb_w * fb_h;
        const unsigned int texLastPoint = ((fb_w * glyphScale) * (fb_h * glyphScale) * pixelSize);
#endif
        for (unsigned int i=0; i<fb_h; i++, texIdx+=texSubRowStride) {
            for (unsigned int j=0; j<fb_w; j++, srcIdx++, texIdx+=dstPointStride) {
                uint8_t value = *(fb + srcIdx);
                PIXEL_TYPE rgb = (((PIXEL_TYPE)(colormap[value].red)   << SHIFT_R) |
                                  ((PIXEL_TYPE)(colormap[value].green) << SHIFT_G) |
                                  ((PIXEL_TYPE)(colormap[value].blue)  << SHIFT_B));

                unsigned int dstIdx = texIdx;

                // perform "eight patch" on adjacent pixels
                if (rgb) {
                    EightPatchArgs_s args = {
                        .parent = parent,
                        .pixelSize = pixelSize,
                        .glyphScale = glyphScale,
                        .fb_h = fb_h,
                        .fb_w = fb_w,
                        .srcIdx = srcIdx,
                        .dstIdx = dstIdx,
                    };
                    _eightpatch_opaquePixelHaloFilter(args);
                }
            }
        }
#ifndef NDEBUG
        assert(srcIdx == srcLastPoint);
        assert(texIdx == texLastPoint);
#endif
    }

    parent->texDirty = true;
}

void glhud_renderDefault(GLModel *parent) {

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(parent->vaoName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, parent->posBufferName);

    GLsizei posTypeSize      = getGLTypeSize(parent->positionType);
    GLsizei texcoordTypeSize = getGLTypeSize(parent->texcoordType);

    // Set up parmeters for position attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX, // What attibute index will this array feed in the vertex shader (see buildProgram)
                          parent->positionSize, // How many elements are there per position?
                          parent->positionType, // What is the type of this data?
                          GL_FALSE, // Do we want to normalize this data (0-1 range for fixed-pont types)
                          parent->positionSize*posTypeSize, // What is the stride (i.e. bytes between positions)?
                          0); // What is the offset in the VBO to the position data?
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Set up parmeters for texcoord attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the texcoord VBO to VAO
    glBindBuffer(GL_ARRAY_BUFFER, parent->texcoordBufferName);
    glVertexAttribPointer(TEXCOORD_ATTRIB_IDX, // What attibute index will this array feed in the vertex shader (see buildProgram)
                          parent->texcoordSize, // How many elements are there per texture coord?
                          parent->texcoordType, // What is the type of this data in the array?
                          GL_TRUE, // Do we want to normalize this data (0-1 range for fixed-point types)
                          parent->texcoordSize*texcoordTypeSize, // What is the stride (i.e. bytes between texcoords)?
                          0); // What is the offset in the VBO to the texcoord data?
    glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, parent->elementBufferName);
#endif

    // Draw the object
    _HACKAROUND_GLDRAW_PRE();
    glDrawElements(parent->primType, parent->numElements, parent->elementType, 0);
    GL_ERRLOG("glhudparent render");
}

void glhud_destroyDefault(GLModel *parent) {
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    if (!hudElement) {
        return;
    }

    if (hudElement->tpl) {
        FREE(hudElement->tpl);
    }

    if (hudElement->pixels) {
        FREE(hudElement->pixels);
    }

    FREE(parent->custom);
}

void glhud_screenToModel(const float x, const float y, const int screenW, const int screenH, float *centerX, float *centerY) {
    *centerX = (x/(screenW>>1))-1.f;
    *centerY = ((screenH-y)/(screenH>>1))-1.f;
}

void glhud_quadModelToScreen(const GLModel *model, const int screenW, const int screenH, float screenCoords[4]) {
    float x0 = 1.0;
    float y0 = 1.0;
    float x1 = -1.0;
    float y1 = -1.0;

#warning NOTE: we possibly should use matrix calculations (but assuming HUD elements are identity/orthographic for now)
    GLfloat *positions = (GLfloat *)(model->positions);
    unsigned int stride = model->positionSize;
    unsigned int len = model->positionArraySize/getGLTypeSize(model->positionType);
    for (unsigned int i=0; i < len; i += stride) {
        float x = (positions[i] + 1.f) / 2.f;
        if (x < x0) {
            x0 = x;
        }
        if (x > x1) {
            x1 = x;
        }
        float y = (positions[i+1] + 1.f) / 2.f;
        LOG("\tmodel x:%f, y:%f", x, y);
        if (y < y0) {
            y0 = y;
        }
        if (y > y1) {
            y1 = y;
        }
    }

    // OpenGL screen origin is bottom-left (Android is top-left)
    float yFlip0 = screenH - (y1 * screenH);
    float yFlip1 = screenH - (y0 * screenH);

    screenCoords[0] = x0 * screenW;
    screenCoords[1] = yFlip0;
    screenCoords[2] = x1 * screenW;
    screenCoords[3] = yFlip1;
}

float glhud_getTimedVisibility(struct timespec timingBegin, float minAlpha, float maxAlpha) {

    struct timespec now = { 0 };

    clock_gettime(CLOCK_MONOTONIC, &now);
    float alpha = minAlpha;
    struct timespec deltat = timespec_diff(timingBegin, now, NULL);
    if (deltat.tv_sec == 0) {
        alpha = maxAlpha;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < 0) {
                alpha = 0;
            }
        }
    }

    return alpha;
}

