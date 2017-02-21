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
};

} // end namespace camera

#endif