#include "camera_usb/cam_usb.h"

enum CamMenu {MAIN, CONFIGURATION, IMAGE_REQUESTING};

void printMainMenu() {
    std::cout << "Menu Main" << std::endl;
    std::cout << "1. Configuration" << std::endl;
    std::cout << "2. Image requesting" << std::endl;
    std::cout << "3. Exit" << std::endl;
}

void printConfigurationMenu() {
    std::cout << "Menu v4l2" << std::endl;
    std::cout << "1. List capabilities" << std::endl;
    std::cout << "2. List controls" << std::endl;
    std::cout << "3. List image format" << std::endl;
    std::cout << "4. List stream parameters" << std::endl;
    std::cout << "5. Back" << std::endl;
}

void printImageRequestingMenu() {
    std::cout << "Menu GStreamer" << std::endl;
    std::cout << "1. Request image" << std::endl;
    std::cout << "2. Store image to file" << std::endl;
    std::cout << "3. Back" << std::endl;
}

int getRequest(int start, int stop) {
    int value = 0;;
    while(value < start || value > stop) {
        std::cout << "Choose (" << start << " - " << stop << "): ";
        if(scanf("%d", &value) != 1) {
            std::cout << "Reading error" << std::endl;
        }
    }
    return value;
}

int main(int argc, char* argv[]) 
{
    using namespace camera;

    if((argc == 2 && strcmp(argv[1], "-h") == 0) || argc < 1 || argc > 2) {
        std::cout << "Small cam_usb test program to list camera-informations and retrieve images." << std::endl;
        std::cout << "Pass the device to use, otherwise the default one '/dev/video0' is used." << std::endl;
        return 0;
    }

    std::string device = "/dev/video0";

    if(argc == 2) {
        device = argv[1];
    }

    int ret = 0;
    CamMenu cam_menu = MAIN;
    CamConfig* cam_config = NULL;
    CamGst* cam_gst = NULL;
    std::vector<uint8_t> buffer;

    while(true) {
        switch(cam_menu) {
            case MAIN: {
                printMainMenu();
                ret = getRequest(1, 3);
                switch(ret) {
                    case 1: {
                        cam_menu = CONFIGURATION;
                        cam_config = new CamConfig(device);
                        break;
                    }
                    case 2: {
                        cam_menu = IMAGE_REQUESTING;
                        cam_gst = new CamGst(device);
                        cam_gst->createDefaultPipeline(true, 640, 480, 10, 80);
                        cam_gst->startPipeline();
                        break;
                    }
                    case 3: {
                        return 0;
                    }
                }
                break;
            }
            case CONFIGURATION: {
                printConfigurationMenu();
                ret = getRequest(1, 5);
                switch(ret) {
                    case 1: {
                        cam_config->readCapability();
                        cam_config->listCapabilities();
                        break;
                    }
                    case 2: {
                        cam_config->readControl();
                        cam_config->listControls();
                        break;
                    }
                    case 3: {
                        cam_config->readImageFormat();
                        cam_config->listImageFormat();
                        break;
                    }
                    case 4: {
                        cam_config->readStreamparm();
                        cam_config->listStreamparm();
                        break;
                    }
                    case 5: {
                        cam_menu = MAIN;
                        delete cam_config;
                        cam_config = NULL;
                        break;
                    }
                }
                break;
            }
            case IMAGE_REQUESTING: {
                printImageRequestingMenu();
                ret = getRequest(1, 3);
                switch(ret) {
                    case 1: {
                        cam_gst->createDefaultPipeline(true, 0, 0, 0, 24, base::samples::frame::MODE_JPEG, 80);
                        cam_gst->startPipeline();
                        if(!cam_gst->getBuffer(buffer, true, 2000)) {
                            std::cout << "Image could not be requested" << std::endl;
                        } else {
                            std::cout << "Image requested (" << buffer.size() << " bytes)" << std::endl;
                        }
                        cam_gst->stopPipeline();
                        cam_gst->deletePipeline();
                        break;
                    }
                    case 2: {
                        if(buffer.empty()) {
                            std::cout << "Request an image first" << std::endl;
                        } else {
                            std::string file_name("test_img.jpg");
                            cam_gst->storeImageToFile(buffer, file_name);
                        }
                        break;
                    }
                    case 3: {
                        cam_menu = MAIN;
                        delete cam_gst;
                        cam_gst = NULL;
                        break;
                    }
                }
                break;
            }
            default: {
                std::cout << "Unknown answer" << std::endl; 
            }
        }
    }
}

