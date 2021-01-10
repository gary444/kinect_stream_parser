#include <iostream>
#include <fstream>
#include <stdio.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core_c.h>

using namespace cv;

#define RGB 1
#define CONVERT_DEPTH_TO_16_BIT 0


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


// converts back from float metres to uint16_t millimetres
uint16_t*
convertTo16BitMM(float* in, unsigned w, unsigned h){
  static uint16_t* out = new uint16_t [w*h];

  for(unsigned idx = 0; idx != w*h; ++idx){
    const float mms = 1000.f * in[idx];
    out[idx] = uint16_t(mms);
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


bool should_export_image(const uint32_t frame){
    // return (frame > 42 && frame < 82 && (frame%5) == 4 );
    return (frame == 45 );
}


std::vector<float> get_list_from_args(char** begin, char** end, const std::string& option, const int num_args){

    std::vector<float> rtn_vec (num_args);

    char** it = std::find(begin, end, option);
    int elements = 0;
    if (it != end && it+1 != end){
        while (++it != end && elements < num_args) {
            rtn_vec[elements++] = atof(*it);
        }
    }
    if (elements < num_args-1){
        throw std::logic_error("not enough elements found for cmd option: " + option);
    }
    return rtn_vec;
}

uint32_t num_frames = 0;
uint32_t target_frame = 0;
uint32_t start_frame = 0;
uint32_t end_frame = 0;
bool TARGET_FRAME_ONLY;
bool OUTPUT_SEQUENCE;

std::string depth_output_dir;
std::string colour_output_dir;


bool should_export_frame(const uint32_t frame_id){

    if (TARGET_FRAME_ONLY && frame_id == target_frame) return true;

    if (OUTPUT_SEQUENCE && frame_id >= start_frame && frame_id < end_frame) return true;

    if (!TARGET_FRAME_ONLY && !OUTPUT_SEQUENCE) return true;

    return false;
}

std::string getDepthOutBasePath(const uint32_t frame_id) {
    if (!OUTPUT_SEQUENCE) return depth_output_dir;
    else return depth_output_dir + "/t" + std::to_string(frame_id) + "/";
}

std::string getColourOutBasePath(const uint32_t frame_id) {
    if (!OUTPUT_SEQUENCE) return colour_output_dir;
    else return colour_output_dir + "/t" + std::to_string(frame_id) + "/";
}


int main(int argc, char** argv )
{


    if (
        cmd_option_exists(argv, argv+argc, "-h") 
        || !cmd_option_exists(argv, argv+argc, "-i") 
        // || !cmd_option_exists(argv, argv+argc, "-n") 
        || argc < 2
          )
    {
        // printf("usage: parse_image <Stream_Path>\n");
        std::cout << "Read kinect streams and output images\n\n"
                  << "\t -i: input file path" << std::endl
                  << "\t -i: out file path base" << std::endl
                  << "\t -qhd: colour texture resolution is 2560x1440. If not provided, HD 1280x720 is assumed" << std::endl
                  << "\t -j: input images are jpegs" << std::endl
                  << "\t -p: create pngs" << std::endl
                  << "\t -m: create masked jpegs to test size" << std::endl
                  << "\t -n: number of time steps (=1)" << std::endl
                  << "\t -t: target frame - just export one step" << std::endl
                  << "\t -seq: [START END) output a sequence of frames from START to END-1" << std::endl
                  ;
        return -1;
    }

    const std::string inpath   = get_cmd_option(argv, argv+argc, "-i"); 
    const bool        QHD      = cmd_option_exists(argv, argv+argc, "-qhd"); 
    const bool        IN_JPGS  = cmd_option_exists(argv, argv+argc, "-j"); 
    const bool        PNGS     = cmd_option_exists(argv, argv+argc, "-p"); 
    const bool        MASKS    = cmd_option_exists(argv, argv+argc, "-m"); 


    if (cmd_option_exists(argv,argv+argc,"-n")){
        num_frames = atoi( get_cmd_option(argv, argv+argc, "-n") ); 
    }
    
    TARGET_FRAME_ONLY = cmd_option_exists(argv, argv+argc, "-t"); 
    if (TARGET_FRAME_ONLY){
        target_frame = atoi( get_cmd_option(argv, argv+argc, "-t") ); 
    }

    OUTPUT_SEQUENCE = cmd_option_exists(argv, argv+argc, "-seq"); 

    if (OUTPUT_SEQUENCE){
        std::vector<float> args = get_list_from_args(argv, argv+argc, "-seq", 2);
        start_frame = uint32_t(args[0]); 
        end_frame = uint32_t(args[1]);

        std::cout << "Will export sequence from " << start_frame << " to " << end_frame << std::endl; 
    }

    num_frames = std::max(num_frames, std::max(target_frame+1, end_frame));

    depth_output_dir = "../images/kinect_textures/";
    colour_output_dir = "../images/kinect_textures/";

    if (cmd_option_exists(argv, argv+argc, "-do")) {
        depth_output_dir = get_cmd_option(argv,argv+argc,"-do");
    }

    if (cmd_option_exists(argv, argv+argc, "-co")) {
        colour_output_dir = get_cmd_option(argv,argv+argc,"-co");
    }

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

    const uint32_t NUM_CAMS = 4;

    if (PNGS){


        for (uint32_t i = 0; i < num_frames; ++i)
        {

            std::cout << "Reading frame " << i << std::endl;

            for (int n = 0; n < NUM_CAMS; ++n){
                file.read(reinterpret_cast<char*> ( dimg_buffer.data() ), dimg_size_bytes);

                Mat dimage_out (dimg_height, dimg_width, CV_16UC1, convertTo16Bit(dimg_buffer.data(), dimg_width, dimg_height ));

                if ( should_export_frame(i) ) {

                    // write depth images as PNG
                    // imwrite("../images/kinect_textures/out_d_t" + std::to_string(i) + "_c"  + std::to_string(n) + ".png", dimage_out);

                    // write depth images as pure floats
                    const std::string path = getDepthOutBasePath(i) + "/out_d_t" + std::to_string(i) + "_c"  + std::to_string(n) + ".depth";
                    std::ofstream ofile (path, std::ios::binary);
                    ofile.write(reinterpret_cast<char*> (dimg_buffer.data()), sizeof(float) * dimg_width * dimg_height);
                    ofile.close();
                    
                    if (!ofile.good()){
                        std::cout << "Error creating file " << path << std::endl;
                        throw std::exception();
                    }


                } 


            }



            for (int n = 0; n < NUM_CAMS; ++n){

                Mat image_out;
                
                if (IN_JPGS){

                    std::size_t jpeg_size;
                    file.read(reinterpret_cast<char*> ( &jpeg_size ), sizeof(std::size_t));

                    // std::cout << "Size of image: " << jpeg_size << std::endl;

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

                if ( should_export_frame(i) ) {
                    if (!imwrite(getColourOutBasePath(i) + "/out_t_" + std::to_string(i) + "_c"  + std::to_string(n) + ".png", image_out)){
                        std::cout << "Couldn't save image" << std::endl;
                    }
                } 


            }

            if(file.peek()==EOF) break;
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

        for (int i = 0; i < num_frames; ++i)
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