#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "zdata.h"
#include "render.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

int strPos(char *tofind, char *findin, uint start)
{
    // Find a string in another string and return its position.
    // return -1 if nothing found.
    // This is just a copy and paste of instr used in zconn.c but I couldn't be bothered to generalise it into a string
    // specific file so I just copy / pasted / and changed the name to create a namespace separation.
    // TODO: Create a string file and put the function in there.
    int i, j; // Loop itterators

    if (start > strlen(findin) - 1)
        return -1;

    for (i = start, j = 0; findin[i] != '\0' && tofind[j] != '\0'; i++)
    {
        if (findin[i] == tofind[j])
            j++;
        else
            j = 0;
    }
    if (j == 0)
        return -1;
    else if (tofind[j] == '\0')
        return i - j;
    return -1; // ToFind is beyond the limit of FindIn.
}

/**
 * Set a single pixel in a bitmap
 * @param [in]  d       bitmap data. assumes three channel (RBG)
 * @param [in]  w       bitmap width
 * @param [in]  h       bitmap height
 * @param [in]  x       x position of the set pixel (top left is 0,0)
 * @param [in]  y       y position of the set pixel (top left is 0,0)
 * @param [in]  r       byte representation of the red channel of the pixel to be set.
 * @param [in]  g       byte representation of the green channel of the pixel to be set.
 * @param [in]  b       byte representation of the blue channel of the pixel to be set.
 * */
void setPixel(char *d, unsigned int w, unsigned int h, unsigned int x, unsigned int y, char r, char g, char b)
{
    int bpp = 3; // bytes per pixel.
    int pos = y * w * bpp;
    pos += x * bpp;
    d[pos++] = r;
    d[pos++] = g;
    d[pos] = b;
}

/**
 * Set a line in a bitmap
 * @param [in]  d       bitmap data. assumes three channel (RBG)
 * @param [in]  w       bitmap width
 * @param [in]  h       bitmap height
 * @param [in]  x0      x position of the start of the line
 * @param [in]  y0      y position of the start of the line
 * @param [in]  x1      x position of the end of the line
 * @param [in]  y1      y position of the end of the line
 * @param [in]  r       byte representation of the red channel of the line
 * @param [in]  g       byte representation of the green channel of the line
 * @param [in]  b       byte representation of the blue channel of the line
 * */
void drawLine(char *d, unsigned int w, unsigned int h, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, char r, char g, char b)
{
    // Both ends of the line have been resolved. Draw them on the bitmap
    // https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;)
    {
        setPixel(d, w, h, x0, y0, r, g, b);

        if (x0 == x1 && y0 == y1)
            break;
        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}

/**
 * Write text into an image bitmap
 * @param [in]  text    text to be written (null terminated)
 * @param [in]  d       bitmap data. assumes three channel (RBG)
 * @param [in]  w       bitmap width
 * @param [in]  h       bitmap height
 * @param [in]  fs      font size in pixels
 * @param [in]  x       x position of the text reference box. Writting from left to right this would be the top left of the text box. Centre aligned, this would be the top centre of the text box.
 * @param [in]  y       y position of the text reference box. Writting from left to right this would be the top left of the text box. Centre aligned, this would be the top centre of the text box.
 * @param [in]  maxw    max width of the text box.
 * */
void writeText(char *text, char *d, unsigned int w, unsigned int h, unsigned int fs, unsigned int x, unsigned int y, int maxw)
{

    // Write some text for each host.
    // See example code for Hello World in stb_truetype.h
    const int bpp = 3; // Bytes per pixel.
    int x0, y0, x1, y1;
    int i, j;
    float scale, xpos = 2; // leave a little padding in case the character extends left
    int ascent, baseline, ch = 0, advance, lsb;

    // Load the font.
    char *buffer = malloc(7 << 18); // file's 108.2kB, this will be big enough.
    FILE *f = fopen("fonts/LiberationMono-Regular.ttf", "rb");
    fread(buffer, 1, 1000000, f);
    fclose(f);
    stbtt_fontinfo font;
    stbtt_InitFont(&font, buffer, 0);

    // Retrieve variables that are constant for the entire font set
    scale = stbtt_ScaleForPixelHeight(&font, fs);
    stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
    baseline = (int)(ascent * scale);

    // Get details for the ellipse (...) character which will be used if the requested text will not fit in the maximum width.
    stbtt_GetCodepointHMetrics(&font, 0x2026, &advance, &lsb);
    int ew = ceil(advance * scale); // Elipse width

    if (x + maxw > w)
        maxw = w - x;

    if (ew > maxw)
    {
        free(buffer);
        return;
    }

    while (text[ch])
    {
        int bmpW, bmpH, bmpXOff, bmpYOff;
        int last = 0;
        stbtt_GetCodepointHMetrics(&font, text[ch], &advance, &lsb);
        unsigned char *glyph;

        // There must be enough room for an ellipses unless we are adding the last character and there is enough room for that instead.
        if (text[ch + 1] && xpos + (advance * scale) + ew > maxw)
        {
            // If we insert the current character then there is not enough space for the elipses after it.
            glyph = stbtt_GetCodepointBitmap(&font, scale, scale, 0x2026, &bmpW, &bmpH, &bmpXOff, &bmpYOff);
            last = 1; // make this the last character.
        }
        else if (xpos + (advance * scale) > maxw)
        {
            // Last character, but not enough room for it, we have to add an elipses
            glyph = stbtt_GetCodepointBitmap(&font, scale, scale, 0x2026, &bmpW, &bmpH, &bmpXOff, &bmpYOff);
        }
        else
        {
            // plenty of room. add the character.
            glyph = stbtt_GetCodepointBitmap(&font, scale, scale, text[ch], &bmpW, &bmpH, &bmpXOff, &bmpYOff);
        }

        int pos = 0;
        for (i = 0; i < bmpH; i++)
        {
            int yAdj = (i + baseline + bmpYOff + y); // Y position of the bmp adjusted against the baseline
            if (yAdj < 0 || yAdj > h)
                break;                                  // outside of confines of bitmap on y axis.
            pos = ((yAdj * w) + floor(xpos) + x) * bpp; // (yAdj * w) is used to work out which row we are on. floor(xpos) gives us the x position in the desired row.
            for (j = 0; j < bmpW; j++)
            {
                d[pos++] = ~glyph[(i * bmpW) + j];
                d[pos++] = ~glyph[(i * bmpW) + j];
                d[pos++] = ~glyph[(i * bmpW) + j];
            }
        }
        if (last == 1)
            break;
        xpos += (advance * scale);
        if (text[ch + 1])
            xpos += scale * stbtt_GetCodepointKernAdvance(&font, text[ch], text[ch + 1]);
        ++ch;
        free(glyph);
    }
    free(buffer);
}

/**
 * Draw a single host into an image bitmap
 * @param [in]  host    host to be drawn
 * @param [in]  d       bitmap data. assumes three channel (RBG)
 * @param [in]  w       bitmap width
 * @param [in]  h       bitmap height
 * @param [in]  x       x position of the host image reference box. Top left corner typically. 
 * @param [in]  y       y position of the host image reference box. Top left corner typically.
 * @param [in]  maxw    max width of the drawn host
 * @param [in]  maxh    max height of the drawn host
 * @param [in]  align   alignment of the image compared to the host image reference box. 0 = top left align, 1 = centre middle align.
 * */
void drawHost(struct host *host, char *d, unsigned int w, unsigned int h, int x, int y, int maxw, int maxh, unsigned short int align)
{
    int bpp = 3;                    // bytes per pixes
    int sprx, spry, sprn;           // Sprite x (width), y (height), n ('comp' which is 3 for RGB, 4 for RGBA (Specific to stb library)).
    int imgw, imgh;                 // final image width and height after resize.
    int stride;                     // the stride in data required to go from the end of data on one row to the start of the data on another row.
    int srcPos = 0, tarPos, rowPos; // data position indicators (relative pointers);
    int i, j;
    float srcar, tarar;  // aspect ratio source and target
    unsigned char *data; // Source image that will be used to represent the host.
    if (align > 1)
    {
        fprintf(stderr, "drawHost alignment invalid\n");
        return;
    }

    // Load the switch image. Really dumb hardcoded solution here just to see if everything works. I will need to
    // 'generalise' this if I even put this into production, along with caching images between calls etc.

    char *sysDesc = malloc(strlen(host->sysDesc) +1);
    strcpy(sysDesc, host->sysDesc); // Copy System Description
    for (i=0; sysDesc[i];i++)
    sysDesc[i] = tolower(sysDesc[i]);// Convert System Description to lower

    if (strPos("xc206", sysDesc, 0) > -1)
        data = stbi_load("images/XC206-2SFP_96.jpg", &sprx, &spry, &sprn, 0);
    else if (strPos("xr524", sysDesc, 0) > -1)
        data = stbi_load("images/XR524-8C.jpg", &sprx, &spry, &sprn, 0);
    else if (strPos("xc208", sysDesc, 0) > -1)
        data = stbi_load("images/XC208.jpg", &sprx, &spry, &sprn, 0);
    else
        data = stbi_load("images/generic_switch.jpg", &sprx, &spry, &sprn, 0);

    // Set resize target based on target bounding box.
    srcar = (float)sprx / spry;
    tarar = (float)maxw / maxh;
    if (srcar > tarar)
    {
        // limiting factor is the width of the bounding box
        imgw = maxw;
        imgh = floor(imgw / srcar);
    }
    else
    {
        // limiting factory is the height of the bounding box
        imgh = maxh;
        imgw = floor(imgh * srcar);
    }

    // reposition the x and y co ordinates to where the top left of the image needs to be drawn.
    // only need to do this if the target alignment is not top left align already.
    if (align == 1)
    {
        x = x - floor(imgw / 2.0);
        y = y - floor(imgh / 2.0);
    }

    x = (x < 0) ? 0 : x; // Prevent x < 0;
    y = (y < 0) ? 0 : y; // Prevent y < 0;

    // resize image.
    unsigned char *resized = malloc(imgw * imgh * bpp);
    stbir_resize_uint8(data, sprx, spry, 0, resized, imgw, imgh, 0, bpp); // resize the host image.

    stride = (w - imgw) * bpp;

    tarPos = y * w * bpp;
    tarPos += x * bpp;

    while (srcPos < imgw * imgh * bpp)
    {
        rowPos = 0;
        while (rowPos < (imgw * bpp))
        {
            d[tarPos++] = resized[srcPos++];
            rowPos++;
        }

        tarPos += stride;
    }
    stbi_image_free(data);
    free(resized);
}

void renderHL(struct hostLink *hl, struct padding *pads)
{
    // Render a host link collection to bitmap file.
    int i, j, k;      // loop itterators
    int w = 0, h = 0; // width and height
    struct host *host;
    char *name = "hostlinks.bmp";

    for (i = 0; i < hl->hosts.count; i++)
    {
        host = &hl->hosts.hosts[i];
        if (host->xPos + host->w > w)
            w = host->xPos + host->w;
        if (host->yPos + host->h > h)
            h = host->yPos + host->h;
    }

    w += pads->right;
    h += pads->bottom;

    unsigned char *d = malloc((sizeof d * (h * w) * 3));

    // Set the background of the image. Bytes per pixel = 3.
    for (i = 0; i < h * w * 3; i++)
        d[i] = 255;

    // Output the host data.
    for (i = 0; i < hl->hosts.count; i++)
    {
        host = &hl->hosts.hosts[i];

        int maxImgH = 80; // dimensions of host image.
        int maxImgW = 100;
        int imgX = host->xPos + floor(maxImgW / 2.0); // reference position for the host image
        int imgY = host->yPos + floor(maxImgH / 2.0);

        // Draw the host itself
        drawHost(host, d, w, h, imgX, imgY, maxImgW, maxImgH, 1); // centre middle align
        //drawHost(host, d, w, h, host->xPos, host->yPos, 100, 100, 0);   // top left align setup

        // Write the text for the hosts.
        writeText(host->name, d, w, h, 15, host->xPos, host->yPos + host->h - 15, host->w);
    }

    // Draw lines between hosts.
    int x0, y0, x1, y1; // Start and end x,y positions for the line.
    int n0, n1;         // connection made
    for (i = 0; i < hl->links.count; i++)
    {
        n0 = 0;
        n1 = 0;
        for (j = 0; j < hl->hosts.count; j++)
        {
            if (hl->links.links[i].a.hostId == hl->hosts.hosts[j].id)
            {
                // link a side matches current host.
                // x and y should be in centre of host.
                x0 = hl->hosts.hosts[j].xPos + (hl->hosts.hosts[j].w / 2);
                y0 = hl->hosts.hosts[j].yPos + (hl->hosts.hosts[j].h / 2);
                n0++;
            }
            else if (hl->links.links[i].b.hostId == hl->hosts.hosts[j].id)
            {
                // link b side matches current host.
                // x and y should be in centre of host.
                x1 = hl->hosts.hosts[j].xPos + (hl->hosts.hosts[j].w / 2);
                y1 = hl->hosts.hosts[j].yPos + (hl->hosts.hosts[j].h / 2);
                n1++;
            }
            if (n0 > 0 && n1 > 0)
                break;
        }
        if (n0 > 0 && n1 > 0)
        {
            // Cheat: to draw the lines in the middle of the images, move the y positions up 10 pixels for both targets.
            // this is because the host 'box' has 80px given over to the image and 20px given over to the text.
            y0 -= 10; // uncomment for centre middle align
            y1 -= 10; // uncomment for centre middle align
            drawLine(d, w, h, x0, y0, x1, y1, 0, 0, 0);
        }
    }

    int bpp = 3; // Bytes per pixel
    stbi_write_bmp(name, w, h, bpp, d);

    free(d);
}
