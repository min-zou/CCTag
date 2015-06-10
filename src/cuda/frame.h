#pragma once

#include <cuda_runtime.h>
#include <assert.h>
#include <string>

#include <opencv2/core/cuda_types.hpp>

#include "../cctag/params.hpp"

namespace cv {
    namespace cuda {
        typedef PtrStepSz<int16_t>  PtrStepSz16s;
        typedef PtrStepSz<uint32_t> PtrStepSz32u;
        typedef PtrStep<int16_t>    PtrStep16s;
        typedef PtrStep<uint32_t>   PtrStep32u;
    }
};

namespace popart {

typedef cudaEvent_t FrameEvent;

/*************************************************************
 * FrameTexture
 * Used by Frame to perform scaling with bilinear interpolation
 * and some other transformations.
 *************************************************************/
class FrameTexture
{
public:
    enum Kind {
        normalized_uchar_to_float
    };
public:
    FrameTexture( const cv::cuda::PtrStepSzb& plane );
    ~FrameTexture( );

    inline cudaTextureObject_t getTex( ) {
        return _texture;
    }

private:
    void makeTex_Normalized_uchar_to_float( const cv::cuda::PtrStepSzb& plane );

private:
    Kind                _kind;
    cudaTextureObject_t _texture;
    cudaTextureDesc     _texDesc;
    cudaResourceDesc    _resDesc;
};

/*************************************************************
 * Frame
 * The basic structure for managing data stored on the GPU
 *************************************************************/
class Frame
{
public:
    // create continuous device memory, enough for @layers copies of @width x @height
    Frame( uint32_t width, uint32_t height );
    ~Frame( );

    // Copy manually created Gauss filter tables to constant memory
    // implemented in frame_gaussian.cu
    static void initGaussTable( );

    // copy the upper layer from the host to the device
    void upload( const unsigned char* image ); // implicitly assumed that w/h are the same as above

    // Create a texture object this frame.
    // The caller must ensure that the Kind of texture object makes sense.
    void createTexture( FrameTexture::Kind kind );

    void deleteTexture( ); // Delete it. Done anyway in the destructor.

    // initialize this frame from other's normalized texture
    void fillFromTexture( Frame& src );
    void fillFromFrame( Frame& src );

    inline cudaTextureObject_t getTex( ) {
        assert( _texture );
        return _texture->getTex( );
    }

    void allocUploadEvent( );
    void deleteUploadEvent( );
    FrameEvent addUploadEvent( );
    void allocDoneEvent( );
    void deleteDoneEvent( );
    FrameEvent addDoneEvent( );
    void streamSync( ); // Wait for the asynchronous ops to finish
    void streamSync( FrameEvent ev ); // Wait for ev to happen (in another stream)

    // return the downscaled sibling "scale". The count it 0-based, 0 is this Frame
    Frame* getScale( uint32_t scale );

    // return width in type_size
    uint32_t getWidth( ) const  { return _d_plane.cols; }
    uint32_t getHeight( ) const { return _d_plane.rows; }
    uint32_t getPitch( ) const  { return _d_plane.step; }

    // implemented in frame_gaussian.cu
    void allocDevGaussianPlane( );

    // implemented in frame_gaussian.cu
    void applyGauss( const cctag::Parameters& param );

    void hostDebugDownload( ); // async

    static void writeDebugPlane1( const char* filename, const cv::cuda::PtrStepSzb& plane );

    template<class T>
    static void writeDebugPlane( const char* filename, const cv::cuda::PtrStepSz<T>& plane );

    void writeHostDebugPlane( std::string filename );
    void hostDebugCompare( unsigned char* pix );

private:
    Frame( );  // forbidden
    Frame( const Frame& );  // forbidden

private:
    cv::cuda::PtrStepSzb _d_plane;
    cv::cuda::PtrStepSzf _d_intermediate;
    cv::cuda::PtrStepSzf _d_smooth;
    // cv::cuda::PtrStepSzf _d_dx;
    // cv::cuda::PtrStepSzf _d_dy;
    cv::cuda::PtrStepSz16s _d_dx;
    cv::cuda::PtrStepSz16s _d_dy;
    cv::cuda::PtrStepSz32u _d_mag;
    cv::cuda::PtrStepSzb   _d_map;
    cv::cuda::PtrStepSzb   _d_edges;
    int2*                  _d_edgelist;
    int4*                  _d_edgelist_2;
    uint32_t               _d_edge_counter;

    unsigned char* _h_debug_plane;
    float*         _h_debug_smooth;
    int16_t*       _h_debug_dx;
    int16_t*       _h_debug_dy;
    uint32_t*      _h_debug_mag;
    unsigned char* _h_debug_map;
    unsigned char* _h_debug_edges;
    FrameTexture*  _texture;
    FrameEvent*    _wait_for_upload;
    FrameEvent*    _wait_done;

    // if we run out of streams (there are 32), we may have to share
    // bool         _stream_inherited;
public:
    cudaStream_t _stream;
};

}; // namespace popart
