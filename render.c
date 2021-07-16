#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zdata.h"
#include "render.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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

    // pixel arrays
    short int red[w][h];
    short int green[w][h];
    short int blue[w][h];

    // set the background
    for (i = 0; i < w; i++)
        for (j = 0; j < h; j++)
        {
            red[i][j] = 255;
            green[i][j] = 255;
            blue[i][j] = 255;
        }

    // Draw the hosts.
    int pos = 0;          // pointer location
    int sprx, spry, sprn; // Sprite x (width), y (height), n ('comp' which is 3 for RGB, 4 for RGBA (Specific to stb library)).
    unsigned char *data = stbi_load("images/XC206-2SFP_96.jpg", &sprx, &spry, &sprn, 0);

    for (i = 0; i < hl->hosts.count; i++)
    {
        pos = 0; // reset pointer
        host = &hl->hosts.hosts[i];
        for (k = host->yPos; (k < host->yPos + host->h) && (k - host->yPos < spry); k++)
            for (j = host->xPos; (j < host->xPos + host->w) && (j - host->xPos < sprx); j++)
            {
                red[j][k] = data[pos++];
                green[j][k] = data[pos++];
                blue[j][k] = data[pos++];
            }
    }

    stbi_image_free(data);

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
            // Both ends of the line have been resolved. Draw them on the bitmap
            // https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
            int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = (dx > dy ? dx : -dy) / 2, e2;

            for (;;)
            {
                //setPixel(x0, y0);
                red[x0][y0] = 0;
                green[x0][y0] = 0;
                blue[x0][y0] = 0;

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
    }

    unsigned char *d = malloc((sizeof d * (h * w)));

    // write to character array
    pos = 0;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
        {
            d[pos++] = (unsigned char)red[i][j];
            d[pos++] = (unsigned char)green[i][j];
            d[pos++] = (unsigned char)blue[i][j];
        }

    // Write some text for each host.
    // See example code for Hello World in stb_truetype.h
    //int x0, y0, x1, y1;
    float scale, xpos = 2; // leave a little padding in case the character extends left
    int ascent, baseline, ch = 0, advance, lsb;
    char *text = "1234";
    char *buffer = malloc(7 << 14); // file's 108.2kB, this will clear it.
    fread(buffer, 1, 1000000, fopen("fonts/LiberationMono-Regular.ttf", "rb"));

    stbtt_fontinfo font;
    stbtt_InitFont(&font, buffer, 0);

    scale = stbtt_ScaleForPixelHeight(&font, 20);
    stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
    baseline = (int)(ascent * scale);

    int charStart, stride;

    while (text[ch])
    {
        int bmpW, bmpH, bmpXOff, bmpYOff;
        unsigned char *glyph = stbtt_GetCodepointBitmap(&font, scale, scale, text[ch], &bmpW, &bmpH, &bmpXOff, &bmpYOff);
        int pos = 0;
        for (i = 0; i < bmpH; i++)
        {
            pos = ((i * w) + xpos) * 3;
            for (j = 0; j < bmpW; j++)
            {
                d[pos++] = ~glyph[(i * bmpW) + j];
                d[pos++] = ~glyph[(i * bmpW) + j];
                d[pos++] = ~glyph[(i * bmpW) + j];
            }
        }
        stbtt_GetCodepointHMetrics(&font, text[ch], &advance, &lsb);
        //xpos += bmpW;     // If using the advance variable you do not need to manually account for the glyph width.
        xpos += (advance * scale);
        if (text[ch + 1])
            xpos += scale * stbtt_GetCodepointKernAdvance(&font, text[ch], text[ch + 1]);
        ++ch;

        /*float x_shift = xpos - (float)floor(xpos); // Not quite sure what this is doing. Looks like it is trying to kill rounding error.
        
        stbtt_GetCodepointBitmapBox(&font, text[ch], scale, scale, &x0, &y0, &x1, &y1);
        int outw, outh;
        outw = x1 - x0;
        outh = y1 - y0;
        char glyph[outh][outw];
        stbtt_MakeCodepointBitmap(&font, &glyph[baseline + y0][0], outw, outh, 0, scale, scale, text[ch]);

        // Write glyph to bitmap.
        int bmppos;
        for (j = 0; j < y1 - y0; j++)
        {
            bmppos = (xpos + (j * w)) * 3;
            for (i = 0; i < x1 - x0; i++)
            {
                d[bmppos++] = glyph[j][i];
                d[bmppos++] = glyph[j][i];
                d[bmppos++] = glyph[j][i];
            }
        }

        xpos += (advance * scale);
        if (text[ch + 1])
            xpos += scale * stbtt_GetCodepointKernAdvance(&font, text[ch], text[ch + 1]);

        ++ch;*/
    }

    free(buffer);

    int bpp = 3; // Bytes per pixel
    stbi_write_bmp(name, w, h, bpp, d);

    free(d);
}
