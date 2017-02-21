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
    
    static uint8_t clip(int value) {
        if(value < 0)
            return 0;
        if(value > 255)
            return 255;
        return value;
    }
    
    static void convertYUYVPixel(uint8_t y, uint8_t u, uint8_t v, 
                                 uint8_t& r, uint8_t& g, uint8_t& b) {
        int y2 = (int)y;
        int u2 = (int)u - 128;
        int v2 = (int)v - 128;

        int r2 = y2 + ((v2 * 37221) >> 15);
        int g2 = y2 - (((u2 * 12975) + (v2 * 18949)) >> 15);
        int b2 = y2 + ((u2 * 66883) >> 15);

        // Cap the values.
        r = clip(r2);
        g = clip(g2);
        b = clip(b2);
    }
    
    // https://www.fourcc.org/fccyvrgb.php#mikes_answer, further informations.
    static void convertYUYV2RGB(uint8_t* yuyv_data, 
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
};

} // end namespace camera

#endif