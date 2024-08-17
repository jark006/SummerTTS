#include "ElementwiseAffine.h"
#include "tts_logger.h"

using Eigen::Map;

struct ELEMENT_AFFINE_DATA_t
{
    int32_t channels_ = 0;
    MatrixXf m_;
    MatrixXf logs_;
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

ElementwiseAffine::ElementwiseAffine(float * modelData, int32_t & offset, int32_t channels)
{
    ELEMENT_AFFINE_DATA_t * elementAffineData = new ELEMENT_AFFINE_DATA_t();
    if(NULL == elementAffineData)
    {
        tts_log(TTS_LOG_ERROR, "ElementwiseAffine: Failed to allocate memory for internal data block\n");
        return;
    }
    
    
    elementAffineData->channels_ = channels;

    int32_t curOffset = offset;
    elementAffineData->m_ = Map<MatrixXf>(modelData+curOffset, elementAffineData->channels_, 1);
    curOffset = curOffset + elementAffineData->channels_;

    elementAffineData->logs_ = Map<MatrixXf>(modelData+curOffset, elementAffineData->channels_, 1);
    curOffset = curOffset + elementAffineData->channels_;

    offset = curOffset;
    priv_ = (void *)elementAffineData;
}

ElementwiseAffine::~ElementwiseAffine()
{
    ELEMENT_AFFINE_DATA_t * elementAffineData = (ELEMENT_AFFINE_DATA_t *)priv_;
    delete elementAffineData;
}

MatrixXf ElementwiseAffine::forward(const MatrixXf & x)
{
    ELEMENT_AFFINE_DATA_t * elementAffineData = (ELEMENT_AFFINE_DATA_t *)priv_;
    
    MatrixXf m =  elementAffineData->m_.transpose();
    MatrixXf logs = elementAffineData->logs_.transpose(); 

    MatrixXf ret = MatrixXf::Zero(x.rows(), x.cols());
    for(int32_t i = 0; i< x.rows(); i++)
    {
        ret.row(i) = (x.row(i) - m).array() * (logs.array()*(-1)).array().exp();
    }

    return ret;
}
