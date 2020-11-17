#include <iostream>
#include <fstream>
#include <stdio.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core_c.h>

using namespace cv;

#define RGB 1

uint16_t*
convertTo16Bit(float* in, unsigned w, unsigned h){
  static uint16_t* out = new uint16_t [w*h];
  static const float norm = 1.0/10.0/*max_distance is 5.0 meter*/;
  for(unsigned idx = 0; idx != w*h; ++idx){
    const float v = norm * in[idx];
    out[idx] = (uint16_t) (65536.0 * v);
  }
  return out;
}

char* get_cmd_option(char** begin, char** end, const std::string & option) {
    char** it = std::find(begin, end, option);
    if (it != end && ++it != end)
        return *it;
    return 0;
}

bool cmd_option_exists(char** begin, char** end, const std::string& option) {
    return std::find(begin, end, option) != end;
}


int main(int argc, char** argv )
{


    if (
        cmd_option_exists(argv, argv+argc, "-h") 
        || !cmd_option_exists(argv, argv+argc, "-i") 
        || !cmd_option_exists(argv, argv+argc, "-n") 
        || argc < 2
          )
    {
        // printf("usage: parse_image <Stream_Path>\n");
        std::cout << "Read kinect streams and output images\n\n"
                  << "\t -i: input file path" << std::endl
                  << "\t -qhd: colour texture resolution is 2560x1440. If not provided, HD 1280x720 is assumed" << std::endl
                  << "\t -j: input images are jpegs" << std::endl
                  << "\t -p: create pngs" << std::endl
                  << "\t -m: create masked jpegs to test size" << std::endl
                  << "\t -n: number of time steps (=1)" << std::endl
                  ;
        return -1;
    }

    const std::string inpath   = get_cmd_option(argv, argv+argc, "-i"); 
    const bool        QHD      = cmd_option_exists(argv, argv+argc, "-qhd"); 
    const bool        IN_JPGS  = cmd_option_exists(argv, argv+argc, "-j"); 
    const bool        PNGS     = cmd_option_exists(argv, argv+argc, "-p"); 
    const bool        MASKS    = cmd_option_exists(argv, argv+argc, "-m"); 
    const uint32_t    num_imgs = atoi( get_cmd_option(argv, argv+argc, "-n") ); 

    uint32_t const img_width  = QHD? 2560 : 1280;
    uint32_t const img_height = QHD? 1440 : 720;
    uint32_t const img_size_bytes = 3 * img_width * img_height;


    uint32_t const dimg_width =  640;
    uint32_t const dimg_height = 576;
    uint32_t const dimg_size_bytes =  dimg_width * dimg_height * sizeof(float);
    
    std::ifstream file(inpath, std::ios::binary);

    std::vector<uint8_t> img_buffer (img_size_bytes);
    std::vector<float>   dimg_buffer(dimg_height*dimg_width);


    // if (IN_JPGS){

    //     std::size_t jpeg_size;
    //     file.read(reinterpret_cast<char*> ( &jpeg_size ), sizeof(std::size_t));

    //     std::cout << "Size of image: " << jpeg_size << std::endl;

    //     img_buffer.resize(jpeg_size);
    //     file.read(reinterpret_cast<char*> ( img_buffer.data() ), jpeg_size);


    //     Mat rawData( 1, jpeg_size, CV_8UC1, img_buffer.data() );
    //     Mat decodedImage  =  imdecode( rawData , cv::IMREAD_UNCHANGED );
    //     if ( decodedImage.data == NULL )   
    //     {
    //         std::cout << "couldnt parse jpeg data" << std::endl;
    //     }
    //     return 0;

    // }


    if (PNGS){


        for (uint32_t i = 0; i < num_imgs; ++i)
        {

            for (int n = 0; n < 4; ++n){
                file.read(reinterpret_cast<char*> ( dimg_buffer.data() ), dimg_size_bytes);
                Mat dimage_out (dimg_height, dimg_width, CV_16UC1, convertTo16Bit(dimg_buffer.data(), dimg_width, dimg_height ));
                imwrite("../images/kinect_textures/out_d_t" + std::to_string(i) + "_c"  + std::to_string(n) + ".png", dimage_out);
            }



            for (int n = 0; n < 4; ++n){

                Mat image_out;
                
                if (IN_JPGS){

                    std::size_t jpeg_size;
                    file.read(reinterpret_cast<char*> ( &jpeg_size ), sizeof(std::size_t));

                    std::cout << "Size of image: " << jpeg_size << std::endl;

                    img_buffer.resize(jpeg_size);
                    file.read(reinterpret_cast<char*> ( img_buffer.data() ), jpeg_size);


                    Mat rawData( 1, jpeg_size, CV_8UC1, img_buffer.data() );
                    image_out  =  imdecode( rawData , cv::IMREAD_UNCHANGED );
                    if ( image_out.data == NULL )   
                    {
                        std::cout << "couldnt parse jpeg data" << std::endl;
                    }
                }
                else {


                    file.read(reinterpret_cast<char*> ( img_buffer.data() ), img_size_bytes);

                    Mat image (img_height, img_width, CV_8UC3, img_buffer.data() );
                    cvtColor(image, image_out, COLOR_BGR2RGB);

                }

                if (!imwrite("../images/kinect_textures/out_t_" + std::to_string(i) + "_c"  + std::to_string(n) + ".png", image_out)){
                    std::cout << "Couldn't save image" << std::endl;
                }

            }


        }

    }

    if (MASKS){

        Mat mask = imread("../images/out_masked.jpg");
        cvtColor(mask, mask, COLOR_RGB2GRAY);

        Mat dmask = imread("../images/out_d_masked.jpg");
        // cvtColor(dmask, dmask, COLOR_RGB2GRAY);


        std::vector<int> compression_params;
        compression_params.push_back(IMWRITE_JPEG_QUALITY);
        compression_params.push_back(100);
        // compression_params.push_back(IMWRITE_JPEG_PROGRESSIVE);
        // compression_params.push_back(1);
        // compression_params.push_back(IMWRITE_JPEG_OPTIMIZE);
        // compression_params.push_back(1);
        // compression_params.push_back(IMWRITE_JPEG_LUMA_QUALITY);
        // compression_params.push_back(30);

        for (int i = 0; i < num_imgs; ++i)
        {
            

            file.read(reinterpret_cast<char*> ( img_buffer.data() ), img_size_bytes);
            file.read(reinterpret_cast<char*> ( dimg_buffer.data() ), dimg_size_bytes);

            if (0 == (i % 4)) {

                Mat image (img_height, img_width, CV_8UC3, img_buffer.data() );
                Mat image_out;
                cvtColor(image, image_out, COLOR_BGR2RGB);
                Mat res;
                bitwise_and(image_out,image_out, res, mask = mask);
                imwrite("../images/col/out_" + std::to_string(i) + ".jpg", res, compression_params);


                Mat dimage_out (dimg_height, dimg_width, CV_16UC1, convertTo16Bit(dimg_buffer.data(), dimg_width, dimg_height ));
                // Mat image_out;
                // cvtColor(image, image_out, COLOR_BGR2RGB);
                // bitwise_and(dimage_out,dimage_out, dimage_out, mask = dmask);

                imwrite("../images/depth/out_d" + std::to_string(i) + ".png", dimage_out);

            }

        }

    }



    return 0;
}