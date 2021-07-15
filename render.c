#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zdata.h"
#include "render.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

    /*// Write the host objects out (as black squares at time of writting)
    for (i = 0; i < hl->hosts.count; i++)
    {
        host = &hl->hosts.hosts[i];
        for (j = host->xPos; j < host->xPos + host->w; j++)
            for (k = host->yPos; k < host->yPos + host->h; k++)
            {
                red[j][k] = 0;
                green[j][k] = 0;
                blue[j][k] = 0;
            }
    }*/

    int pos = 0;          // pointer location
    int sprx, spry, sprn; // Sprite x (width), y (height), n ('comp' which is 3 for RGB, 4 for RGBA (Specific to stb library)).
    unsigned char *data = stbi_load("images/XC206-2SFP_96.jpg", &sprx, &spry, &sprn, 0);

    for (i = 0; i < hl->hosts.count; i++)
    {
        pos = 0; // reset pointer
        host = &hl->hosts.hosts[i];
        for (k = host->yPos; (k < host->yPos + host->h) && (k- host->yPos < spry) ; k++)
            for (j = host->xPos; (j < host->xPos + host->w) && (j- host->xPos < sprx) ; j++)
            {
                red[j][k] = data[pos++];
                green[j][k] = data[pos++];
                blue[j][k] = data[pos++];
            }
    }

    stbi_image_free(data);

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

    int bpp = 3; // Bytes per pixel
    stbi_write_bmp(name, w, h, bpp, d);
    
    free(d);
}
