#include "camera_usb/cam_usb.h"
#include "helpers.h"


//#include <opencv2/core/core.hpp>
//#include <opencv2/highgui/highgui.hpp>

enum CamMenu {MAIN, CONFIGURATION, IMAGE_REQUESTING};

void printMainMenu() {
    std::cout << "Menu Main" << std::endl;
    std::cout << "1. V4L2" << std::endl;
    std::cout << "2. GStreamer" << std::endl;
    std::cout << "3. Exit" << std::endl;
}

void printConfigurationMenu() {
    std::cout << "Menu v4l2" << std::endl;
    std::cout << "1. List capabilities" << std::endl;
    std::cout << "2. List controls" << std::endl;
    std::cout << "3. List image format" << std::endl;
    std::cout << "4. List stream parameters" << std::endl;
    std::cout << "5. Request image using v4l2" << std::endl;
    std::cout << "6. Back" << std::endl;
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

double calculate_fps(timeval start_time_grabbing, int num_received_images) {
    timeval stop_time_grabbing;
    gettimeofday(&stop_time_grabbing, 0);
    double sec = stop_time_grabbing.tv_sec - start_time_grabbing.tv_sec;
    if(sec == 0)
        return 0;
    else
        return num_received_images / sec;
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
    
    std::cout << "Device: " << device << std::endl;
    
    
    // Start conversion test
    /*
    CamUsb cam(device);
    std::vector<CamInfo> cam_info;
    cam.listCameras(cam_info);
    cam.open(cam_info[0]);
    const base::samples::frame::frame_size_t size(640, 480);
    cam.setFrameSettings(size, base::samples::frame::MODE_RGB, 3);
    cam.grab();
    base::samples::frame::Frame frame;
    frame.init(640, 480, 8, base::samples::frame::MODE_RGB);
    cam.retrieveFrame(frame, 2000);
    return 0;
    */
    // End conversion test
    
    
    
    
    int ret = 0;
    CamMenu cam_menu = MAIN;
    CamConfig* cam_config = NULL;
    CamGst* cam_gst = NULL;
    std::vector<uint8_t> buffer;
    
    int numImagesToRequest = 100;
    
    /*
    cv::Mat frame = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::namedWindow("window",CV_WINDOW_AUTOSIZE);
    cv::waitKey(10);
    */
    while(true) {
        switch(cam_menu) {
            case MAIN: {
                printMainMenu();
                ret = getRequest(1, 3);
                switch(ret) {
                    case 1: {
                        cam_menu = CONFIGURATION;
                        cam_config = new CamConfig(device);
                        cam_config->writeImagePixelFormat(640, 480);
                        break;
                    }
                    case 2: {
                        cam_menu = IMAGE_REQUESTING;
                        cam_gst = new CamGst(device);
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
                ret = getRequest(1, 6);
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
                        try {
                            cam_config->initRequesting();
                            
                            timeval start_time;
                            gettimeofday(&start_time, 0);
                            
                            for(int i=0; i<numImagesToRequest; i++) {
                               
                                if(!cam_config->getBuffer(buffer, true, 2000)) {
                                    std::cout << "Image could not be requested" << std::endl;
                                } else {
                                    std::cout << "Image requested (" << buffer.size() << " bytes)" << std::endl;
                                }    
                                
                                /*
                                cv::Mat test = cv::imdecode(buffer, CV_LOAD_IMAGE_COLOR);
                                if (test.empty() == true) {
                                    std::cout << "Empty image: problem to decode the buffer." << std::endl;
                                }
                                frame = test.clone();
                                cv::imshow("window", frame);
                                cv::waitKey(10);
                                */
                                
                            }

                            cam_config->cleanupRequesting();
                            std::cout << "v4l2 image requesting: done " << std::endl;
                            double fps = calculate_fps(start_time, numImagesToRequest); 
                            std::cout << "FPS: " << fps << std::endl;
                                   
                        } catch(std::runtime_error& e) {
                            std::cout << "Error v4l2 image requesting" << std::endl;
                        }
                        break;
                    }
                    case 6: {
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
                        cam_gst->createDefaultPipeline(true, 640, 480, 30, 24, base::samples::frame::MODE_JPEG, 80);
                        cam_gst->startPipeline();
                        
                        timeval start_time;
                        gettimeofday(&start_time, 0);
                        
                        for(int i=0; i<numImagesToRequest; i++) {
                            if(!cam_gst->getBuffer(buffer, true, 2000)) {
                                std::cout << "Image could not be requested" << std::endl;
                            } else {
                                std::cout << "Image requested (" << buffer.size() << " bytes)" << std::endl;
                            }
                            
                            /*
                            cv::Mat test = cv::imdecode(buffer, CV_LOAD_IMAGE_COLOR);
                            if (test.empty() == true) {
                                std::cout << "Empty image: problem to decode the buffer." << std::endl;
                            }
                            frame = test.clone();
                            cv::imshow("window", frame);
                            //cv::waitKey(10); // Does not work with GStreamer?
                            */
                        }
                        
                        cam_gst->stopPipeline();
                        cam_gst->deletePipeline();
                        
                        double fps = calculate_fps(start_time, numImagesToRequest); 
                        std::cout << "FPS: " << fps << std::endl;
                        break;
                    }
                    case 2: {
                        if(buffer.empty()) {
                            std::cout << "Request an image first" << std::endl;
                        } else {
                            std::string file_name("test_img.jpg");
                            Helpers::storeImageToFile(buffer, file_name);
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

