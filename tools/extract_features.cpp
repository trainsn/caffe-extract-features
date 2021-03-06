#include <string>
#include <vector>
#include <math.h>
#include <algorithm>

#include "boost/algorithm/string.hpp"
#include "google/protobuf/text_format.h"

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/net.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/format.hpp"
#include "caffe/util/io.hpp"

#define SIZE 48
#define DIM 4096
#define NUM_PER_IMG 40

using caffe::Blob;
using caffe::Caffe;
using caffe::Datum;
using caffe::Net;
using std::string;

template<typename Dtype>
int feature_extraction_pipeline(int argc, char** argv);
float getMold(const std::vector<float>& vec);
float getSimilarity(const std::vector<float>& lhs, const std::vector<float>& rhs);
float getDistance(const std::vector<float>& lhs, const std::vector<float>& rhs);

int main(int argc, char** argv) {
  freopen("/home/cad/disk/linux/RenderForCNN-master/train/feat_extra/tree2_simi.txt","w",stdout);
  return feature_extraction_pipeline<float>(argc, argv);
}


template<typename Dtype>
int feature_extraction_pipeline(int argc, char** argv) {
  ::google::InitGoogleLogging(argv[0]);
  const int num_required_args = 7;
  if (argc < num_required_args) {
    LOG(ERROR)<<
    "This program takes in a trained network and an input data layer, and then"
    " extract features of the input data produced by the net.\n"
    "Usage: extract_features  pretrained_net_param"
    "  feature_extraction_proto_file  extract_feature_blob_name1[,name2,...]"
    "  save_feature_dataset_name1[,name2,...]  num_mini_batches  db_type"
    "  [CPU/GPU] [DEVICE_ID=0]\n"
    "Note: you can extract multiple features in one pass by specifying"
    " multiple feature blob names and dataset names separated by ','."
    " The names cannot contain white space characters and the number of blobs"
    " and datasets must be equal.";
    return 1;
  }
  int arg_pos = num_required_args;

  arg_pos = num_required_args;
  if (argc > arg_pos && strcmp(argv[arg_pos], "GPU") == 0) {
    LOG(ERROR)<< "Using GPU";
    int device_id = 0;
    if (argc > arg_pos + 1) {
      device_id = atoi(argv[arg_pos + 1]);
      CHECK_GE(device_id, 0);
    }
    LOG(ERROR) << "Using Device_id=" << device_id;
    Caffe::SetDevice(device_id);
    Caffe::set_mode(Caffe::GPU);
  } else {
    LOG(ERROR) << "Using CPU";
    Caffe::set_mode(Caffe::CPU);
  }

  arg_pos = 0;  // the name of the executable
  std::string pretrained_binary_proto(argv[++arg_pos]);

  // Expected prototxt contains at least one data layer such as
  //  the layer data_layer_name and one feature blob such as the
  //  fc7 top blob to extract features.
  /*
   layers {
     name: "data_layer_name"
     type: DATA
     data_param {
       source: "/path/to/your/images/to/extract/feature/images_leveldb"
       mean_file: "/path/to/your/image_mean.binaryproto"
       batch_size: 128
       crop_size: 227
       mirror: false
     }
     top: "data_blob_name"
     top: "label_blob_name"
   }
   layers {
     name: "drop7"
     type: DROPOUT
     dropout_param {
       dropout_ratio: 0.5
     }
     bottom: "fc7"
     top: "fc7"
   }
   */
  std::string feature_extraction_proto(argv[++arg_pos]);
  boost::shared_ptr<Net<Dtype> > feature_extraction_net(
      new Net<Dtype>(feature_extraction_proto, caffe::TEST));
  feature_extraction_net->CopyTrainedLayersFrom(pretrained_binary_proto);

  std::string extract_feature_blob_names(argv[++arg_pos]);
  std::vector<std::string> blob_names;
  boost::split(blob_names, extract_feature_blob_names, boost::is_any_of(","));

  std::string save_feature_dataset_names(argv[++arg_pos]);
  std::vector<std::string> dataset_names;
  boost::split(dataset_names, save_feature_dataset_names,
               boost::is_any_of(","));
  CHECK_EQ(blob_names.size(), dataset_names.size()) <<
      " the number of blob names and dataset names must be equal";
  size_t num_features = blob_names.size();

  for (size_t i = 0; i < num_features; i++) {
    CHECK(feature_extraction_net->has_blob(blob_names[i]))
        << "Unknown feature blob name " << blob_names[i]
        << " in the network " << feature_extraction_proto;
  }

  int num_mini_batches = atoi(argv[++arg_pos]);

  LOG(ERROR)<< "Extracting Features";

  //add by trainsn
  std::vector< std::vector <float> > feat;
  feat.resize(num_mini_batches, std::vector<float>(DIM));

  std::vector<int> image_indices(num_features, 0);
  for (int batch_index = 0; batch_index < num_mini_batches; ++batch_index) {

    feature_extraction_net->Forward(); //fp to get the features
    for (int i = 0; i < num_features; ++i) {
      const boost::shared_ptr<Blob<Dtype> > feature_blob =
        feature_extraction_net->blob_by_name(blob_names[i]);
      int batch_size = feature_blob->num();
      int dim_features = feature_blob->count() / batch_size;
      const Dtype* feature_blob_data;

      //maxCos = -2;
      //minCos = 2;
      std::vector<float> temp_simi;
      for (int n = 0; n < batch_size; ++n) {    //batch_size  = 40 + 1
        feature_blob_data = feature_blob->cpu_data() +
            feature_blob->offset(n);

        if (!n){
            for (int d = 0; d < dim_features; ++d) {
                feat[batch_index][d] = feature_blob_data[d];
                //std::cout << batch_index+1 << " " << n << cosres << std::endl;
              }
        }else {
            std::vector<float> tempFeat(DIM);
            for (int d = 0; d < dim_features; ++d) {
                tempFeat[d] = feature_blob_data[d];
                //std::cout << batch_index+1 << " " << n << cosres << std::endl;
              }
            //std::cout << batch_index+1 << "-" << n << ": " << getSimilarity(feat[batch_index], tempFeat) << std::endl;
            temp_simi.push_back(getSimilarity(feat[batch_index], tempFeat));
        }

        //std::cout << std::endl;
        ++image_indices[i];
      }  // for (int n = 0; n < batch_size; ++n)
      float avg_simi = 0;
      int topk = 4;
      std::sort(temp_simi.begin(), temp_simi.end());
      std::reverse(temp_simi.begin(), temp_simi.end());
      for (size_t k = 0; k < topk; k++)
          avg_simi += temp_simi[k]/ topk;
      std::cout << batch_index+1 << " " << avg_simi << std::endl;
    }  // for (int i = 0; i < num_features; ++i)
  }  // for (int batch_index = 0; batch_index < num_mini_batches; ++batch_index)*/

  //compute the similarity with cos ; add by trainsn
  std::vector< std::vector <float> > cosres;
  std::vector< std::vector <float> > eures;
  cosres.resize(num_mini_batches, std::vector<float>(num_mini_batches));
  eures.resize(num_mini_batches, std::vector<float>(num_mini_batches));
  /*for (int i = 0; i < num_mini_batches; i++){
          for (int j = 0; j < num_mini_batches; j++){
              cosres[i][j] = getSimilarity(feat[i], feat[j]);
              std::cout << exp((cosres[i][j]-0.5)*9)<< " ";
          }
          std::cout << std::endl;
      }*/


  // write the last batch
  for (int i = 0; i < num_features; ++i) {
    LOG(ERROR)<< "Extracted features of " << image_indices[i] <<
        " query images for feature blob " << blob_names[i];
  }

  LOG(ERROR)<< "Successfully extracted the features!";
  return 0;
}

float getMold(const std::vector<float>& vec){   //求向量的模长
    int n = vec.size();
    float sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += vec[i] * vec[i];
    return sqrt(sum);
}

//求两个向量的余弦相似度
float getSimilarity(const std::vector<float>& lhs, const std::vector<float>& rhs){
    int n = lhs.size();
    assert(n == rhs.size());
    float tmp = 0.0;  //内积
    for (int i = 0; i < n; ++i)
        tmp += lhs[i] * rhs[i];
    return tmp / (getMold(lhs)*getMold(rhs));
}

float getDistance(const std::vector<float>& lhs, const std::vector<float>& rhs){
    int n = lhs.size();
    assert(n == rhs.size());
    float tmp = 0.0;
    for (int i = 0; i < n; i++)
        tmp += (lhs[i]-rhs[i]) * (lhs[i]-rhs[i]);
    return sqrt(tmp);
}
