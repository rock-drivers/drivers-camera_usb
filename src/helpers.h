/*
 * \file    helpers.h
 *  
 * \brief   Some general helper functions.
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    17.02.17
 *
 * \author  Stefan.Haase@ground-truth-robotics.de
 */

#ifndef _CAM_V4L2_HELPERS_H_
#define _CAM_V4L2_HELPERS_H_

#include <assert.h>

#include <base/samples/Frame.hpp>

namespace camera 
{

class Helpers {
 public:
    /**
     * Creates YUV2RGB lookup tables.
     */
    Helpers() {
        for(int v=0; v<256; ++v) {
             lookup_v2r[v] = ( (v-128) * 37221 ) >> 15;
        }
        
        for(int u=0; u<256; ++u) {
            for(int v=0; v<256; ++v) {
                lookup_uv2g[u][v] = ( ((u-128) * 12975) + ((v-128) * 18949) ) >> 15;
            }
        }

        for(int u=0; u<256; ++u) {
             lookup_u2b[u] = ((u-128) * 66883) >> 15;
        }

    }

    /**
     * Someone (OpenCV?) does not understand JPEG comment-blocks.
     * Removes comment block to avoid getting 
     * 'Corrupt JPEG data: x extraneous bytes before marker 0xe0.'
     */
    static void removeJpegCommentBlock( base::samples::frame::Frame& frame) {

        if(frame.getFrameMode() == base::samples::frame::MODE_JPEG) {
            std::vector<uint8_t>::iterator it = frame.image.begin();
            std::vector<uint8_t>::iterator it_n = frame.image.begin() + 1;
            for(; it_n != frame.image.end(); it++, it_n++) {
                if(*it == 0xFF && *it_n == 0xFE) {
                    size_t old_length = *(it+2)<<8 | *(it_n+2);
                    // e.g. empty comment block: FF FE 00 02 XX
                    //                           it          it+4
                    frame.image.erase(it, it + (2 + old_length)); 
                    break;
                }
                
                // Start of scan, no comment block found.
                if(*it == 0xFF && *it_n == 0XDA) {
                    break;
                }
            }
        }
    } 
    
    static bool storeImageToFile(std::vector<uint8_t> const& buffer, std::string const& file_name) {
        LOG_DEBUG("storeImageToFile, buffer contains %d bytes, stores to %s", 
                buffer.size(), file_name.c_str());

        if(buffer.empty()) {
            LOG_WARN("Empty buffer passed, nothing will be stored");
            return false;
        }

        FILE* file = NULL;
        file = fopen(file_name.c_str(),"w");
        if(file == NULL) {
            LOG_ERROR("File %s could not be opened, no image will be stored", file_name.c_str());
            return false;
        }
            unsigned int written = fwrite (&buffer[0], 1 , buffer.size(), file);
        if(written != buffer.size()) {
            LOG_ERROR("Only %d of %d bytes could be written", written, buffer.size());
            fclose(file);
            return false;
        }
        fclose(file);
        return true;
    }
    
    uint8_t clip(int value) {
        if(value < 0)
            return 0;
        if(value > 255)
            return 255;
        return value;
    }

    void convertYUYVPixel(uint8_t y, uint8_t u, uint8_t v, 
                          uint8_t& r, uint8_t& g, uint8_t& b) {
                              
        int y2 = (int)y;

        int r2 = y2 + lookup_v2r[(int)v]; // ((v2 * 37221) >> 15);
        int g2 = y2 - lookup_uv2g[(int)u][(int)v];  // (((u2 * 12975) + (v2 * 18949)) >> 15);
        int b2 = y2 + lookup_u2b[(int)u];      // ((u-128) * 66883) >> 15

        // Cap the values.
        r = clip(r2);
        g = clip(g2);
        b = clip(b2);
    }
    
    /**
     * Converts an YUYV image to RGB24.
     * \param yuyv_data Pointer to the YUV data. Four bytes are two pixels, U and V belong
     * to both Ys: YUY'V forms YUV and Y'UV. Ranges on the test system: Y[0,255] U[0,218] V[0,254]
     * \param yuyv_data_length Number of bytes of yuyv_data. Divides by 2 is the number of pixels.
     * \param rgb_buffer Buffer which will receive the RGB pixels.
     * http://stackoverflow.com/questions/37561461/how-to-convert-yuyv-to-rgb-code-to-yuv420-to-rgb
     */
    void convertYUYV2RGB(uint8_t* yuyv_data, 
                                size_t yuyv_data_length, 
                                std::vector<uint8_t>& rgb_buffer) {
        
        assert(yuyv_data_length%4 == 0);
        // YUYV are two bytes per pixel, RGB uses three.
        int rgb_size = (yuyv_data_length / 2) * 3;
        rgb_buffer.resize(rgb_size);
        std::vector<uint8_t>::iterator it = rgb_buffer.begin();
        // Piyel 1:  yuv
        // Pixel 2: y2uv
        uint8_t* y  = yuyv_data;
        uint8_t* u  = y + 1;
        uint8_t* y2 = y + 2;
        uint8_t* v  = y + 3;
        uint8_t r,g,b;
        // y [16,235] uv [16,240]
        for(unsigned int i=0; i < yuyv_data_length/4 && it != rgb_buffer.end(); ++i, y+=4, u+=4, y2+=4, v+=4) {
            /*
            *it = *y  + 1.403 * *v;              it++; // R 38.448 to  
            *it = *y  - 0.344 * *u - 0.714 * *v; it++; // G
            *it = *y  + 1.77  * *u;              it++; // B
            *it = *y2 + 1.403 * *v;              it++; // R2
            *it = *y2 - 0.344 * *u - 0.714 * *v; it++; // G2
            *it = *y2 + 1.77  * *u;              it++; // B2
            */
            convertYUYVPixel(*y, *u, *v, r, g, b);
            *it = r; it++;
            *it = g; it++;
            *it = b; it++;
            
            convertYUYVPixel(*y2, *u, *v, r, g, b);
            *it = r; it++;
            *it = g; it++;
            *it = b; it++;
        }
    }

 private:
    int lookup_v2r[256];
    int lookup_uv2g[256][256];
    int lookup_u2b[256];
};

} // end namespace camera

#endif
