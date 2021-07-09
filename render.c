#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zdata.h"
#include "render.h"
/**
 * Render a bitmap image to file.
 * @param [in]  name    The name of the output bitmap along with extension.
 * @param [in]  w       width of bitmap in pixels
 * @param [in]  h       height of the bitmap in pixels
 * @param [in]  red     2d array of pixels representing the red colour for each pixel
 * @param [in]  green   2d array of pixels representing the green colour for each pixel
 * @param [in]  blue    2d array of pixels representing the blue colour for each pixel
 * */
void renderBmp(char *name, size_t w, size_t h, short int red[w][h], short int green[w][h], short int blue[w][h])
{
    int x, y, r, g, b;
    FILE *f;
    unsigned char *img = NULL;
    int filesize = 54 + 3 * w * h; //w is your image width, h is image height, both int

    img = (unsigned char *)malloc(3 * w * h);
    memset(img, 0, 3 * w * h);

    for (int i = 0; i < w; i++)
    {
        for (int j = 0; j < h; j++)
        {
            x = i;
            y = (h - 1) - j;
            r = red[i][j];
            g = green[i][j];
            b = blue[i][j];
            if (r > 255)
                r = 255;
            if (g > 255)
                g = 255;
            if (b > 255)
                b = 255;
            img[(x + y * w) * 3 + 2] = (unsigned char)(r);
            img[(x + y * w) * 3 + 1] = (unsigned char)(g);
            img[(x + y * w) * 3 + 0] = (unsigned char)(b);
        }
    }

    unsigned char bmpfileheader[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
    unsigned char bmpinfoheader[40] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0};
    unsigned char bmppad[3] = {0, 0, 0};

    bmpfileheader[2] = (unsigned char)(filesize);
    bmpfileheader[3] = (unsigned char)(filesize >> 8);
    bmpfileheader[4] = (unsigned char)(filesize >> 16);
    bmpfileheader[5] = (unsigned char)(filesize >> 24);

    bmpinfoheader[4] = (unsigned char)(w);
    bmpinfoheader[5] = (unsigned char)(w >> 8);
    bmpinfoheader[6] = (unsigned char)(w >> 16);
    bmpinfoheader[7] = (unsigned char)(w >> 24);
    bmpinfoheader[8] = (unsigned char)(h);
    bmpinfoheader[9] = (unsigned char)(h >> 8);
    bmpinfoheader[10] = (unsigned char)(h >> 16);
    bmpinfoheader[11] = (unsigned char)(h >> 24);

    f = fopen(name, "wb");
    fwrite(bmpfileheader, 1, 14, f);
    fwrite(bmpinfoheader, 1, 40, f);
    for (int i = 0; i < h; i++)
    {
        fwrite(img + (w * (h - i - 1) * 3), 3, w, f);
        fwrite(bmppad, 1, (4 - (w * 3) % 4) % 4, f);
    }

    free(img);
    fclose(f);
}

void renderHL(struct hostLink *hl)
{
    // Render a host link collection to bitmap file.
    int i, j, k; // loop itterators
    int w, h;    // width and height
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

    // Output the individual hosts as simple objects
    for (i = 0; i < hl->hosts.count; i++)
    {
        host = &hl->hosts.hosts[i];
        for (j = host->xPos; j < host->xPos + host->w; j++)
            for (k = host->yPos; k < host->yPos + host->h; k++)
            {
                red[i][j] = 0;
                green[i][j] = 0;
                blue[i][j] = 0;
            }
    }

    renderBmp(name, (size_t)w, (size_t)h, red, green, blue);
}
