/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#include "glhudmodel.h"
#include "glvideo.h"

void *glhud_createDefault(void) {
    GLModelHUDElement *custom = (GLModelHUDElement *)calloc(sizeof(GLModelHUDElement), 1);
    if (!custom) {
        return NULL;
    }
    return custom;
}

void glhud_setupDefault(GLModel *parent) {

    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    char *submenu = (char *)(hudElement->tpl);
    const unsigned int cols = hudElement->tplWidth;
    const unsigned int rows = hudElement->tplHeight;
    uint8_t *fb = hudElement->pixels;

    // render template into indexed fb
    interface_printMessageCentered(fb, cols, rows, RED_ON_BLACK, submenu, cols, rows);

    // generate RGBA_8888 from indexed color
    const unsigned int fb_w = hudElement->pixWidth;
    const unsigned int fb_h = hudElement->pixHeight;
    const unsigned int count = fb_w * fb_h;
    for (unsigned int i=0, j=0; i<count; i++, j+=4) {
        uint8_t index = *(fb + i);
        uint32_t rgb = (((uint32_t)(colormap[index].red)   << 0 ) |
                        ((uint32_t)(colormap[index].green) << 8 ) |
                        ((uint32_t)(colormap[index].blue)  << 16));
        if (rgb == 0 && hudElement->blackIsTransparent) {
            // make black transparent
        } else {
            rgb |=      ((uint32_t)0xff                    << 24);
        }
        *( (uint32_t*)(parent->texPixels + j) ) = rgb;
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

