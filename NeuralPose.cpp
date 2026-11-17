#include "NeuralPose.h"

#include <iostream>

#ifdef RPIHT_HAVE_NEURALNET

#include <cmath>
#include <optional>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include <opencv2/imgproc.hpp>

#include "NeuralPoseMath.h"

namespace
{
  constexpr float kMinFaceScore = 0.5f;    // localizer: above = a face
  constexpr float kFollowOverlap = 0.25f;  // localizer and followed box overlap less: a different place
  constexpr double kMaxUncertainty = 20.0; // degrees; a crop without a face gives about 50, a face about 3
  constexpr int kRecheckFrames = 10;       // localizer unsure while a face is followed: ask again every n frames
  constexpr float kMinBox = 16.f;          // pixels; smaller boxes are no usable face

  // One ONNX model: session plus the names of its input and outputs
  struct Net
  {
    Ort::Session session{ nullptr };
    std::string input;
    cv::Size input_size;
    std::vector<std::string> outputs;
    std::vector<const char*> output_names;

    void load(Ort::Env& env, const std::string& path, const Ort::SessionOptions& options)
    {
      session = Ort::Session(env, path.c_str(), options);
      Ort::AllocatorWithDefaultOptions alloc;
      input = session.GetInputNameAllocated(0, alloc).get();
      const auto shape = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
      if (shape.size() != 4 || shape[0] != 1 || shape[1] != 1)
        throw std::runtime_error("unexpected input shape in " + path);
      input_size = cv::Size(int(shape[3]), int(shape[2]));
      for (size_t i = 0; i < session.GetOutputCount(); ++i)
        outputs.push_back(session.GetOutputNameAllocated(i, alloc).get());
      for (const auto& n : outputs)
        output_names.push_back(n.c_str());
    }

    int index(const std::string& name) const
    {
      for (size_t i = 0; i < outputs.size(); ++i)
        if (outputs[i] == name)
          return int(i);
      return -1;
    }

    // 'tensor': CV_32F, input_size, continuous
    std::vector<Ort::Value> run(const Ort::MemoryInfo& memory, cv::Mat& tensor)
    {
      const int64_t shape[4] = { 1, 1, tensor.rows, tensor.cols };
      Ort::Value in = Ort::Value::CreateTensor<float>(memory, tensor.ptr<float>(), tensor.total(), shape, 4);
      const char* in_name = input.c_str();
      return session.Run(Ort::RunOptions{ nullptr }, &in_name, &in, 1, output_names.data(), output_names.size());
    }
  };
}

struct NeuralPose::Impl
{
  Ort::Env env{ ORT_LOGGING_LEVEL_ERROR, "headtracker" };
  Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Net localizer, pose;
  int out_quat = -1, out_box = -1, out_centre = -1, out_uncertainty = -1;
  bool ok = false;

  std::optional<cv::Rect2f> followed;  // face box from the pose network, used for the next frame
  std::optional<cv::Rect2f> found;     // last face box from the localizer
  int since_check = 0;
  cv::Mat grey, small, crop, scaled, tensor;

  // Localizer: probability of a face and its box (pixels of 'grey')
  std::pair<float, cv::Rect2f> localize()
  {
    cv::resize(grey, small, localizer.input_size, 0, 0, cv::INTER_AREA);
    small.convertTo(tensor, CV_32F, 1.0 / 255, -0.5);
    auto out = localizer.run(memory, tensor);
    const float* r = out[0].GetTensorData<float>();
    const float score = 1.f / (1.f + std::exp(-r[0]));
    // corners in -1 ... 1
    const float x0 = (r[1] + 1) * 0.5f * grey.cols, y0 = (r[2] + 1) * 0.5f * grey.rows;
    const float x1 = (r[3] + 1) * 0.5f * grey.cols, y1 = (r[4] + 1) * 0.5f * grey.rows;
    return { score, cv::Rect2f(x0, y0, x1 - x0, y1 - y0) };
  }
};

bool NeuralPose::built_in() { return true; }

NeuralPose::NeuralPose(const std::string& model_dir, int threads)
  : impl_(std::make_unique<Impl>())
{
  try {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(threads);
    options.SetInterOpNumThreads(1);
    options.AddConfigEntry("session.intra_op.allow_spinning", "0");  // no busy waiting between frames
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    impl_->localizer.load(impl_->env, model_dir + "/head-localizer.onnx", options);
    impl_->pose.load(impl_->env, model_dir + "/head-pose-0.5-small.onnx", options);
    impl_->out_quat = impl_->pose.index("quat");
    impl_->out_box = impl_->pose.index("box");
    impl_->out_centre = impl_->pose.index("pos_size");
    impl_->out_uncertainty = impl_->pose.index("rotaxis_scales_tril");
    if (impl_->out_quat < 0 || impl_->out_box < 0 || impl_->out_centre < 0)
      throw std::runtime_error("the pose model has not the expected outputs");
    impl_->ok = true;
  }
  catch (const std::exception& e) {
    std::cerr << "NeuralNet face model: cannot load the models from " << model_dir << ": " << e.what() << std::endl;
  }
}

NeuralPose::~NeuralPose() = default;

bool NeuralPose::ok() const { return impl_->ok; }

void NeuralPose::reset()
{
  impl_->followed.reset();
  impl_->found.reset();
  impl_->since_check = 0;
}

NeuralFace NeuralPose::process(const cv::Mat& frame)
{
  Impl& m = *impl_;
  if (!m.ok || frame.empty())
    return {};
  try {
    if (frame.channels() == 3)
      cv::cvtColor(frame, m.grey, cv::COLOR_BGR2GRAY);
    else
      m.grey = frame;

    // 1. Where to look: the box followed from the last frame, or the localizer's
    bool ask;
    if (!m.followed)
      ask = true;
    else if (!m.found)
      ask = ++m.since_check >= kRecheckFrames;
    else
      ask = box_overlap(*m.found, *m.followed) < kFollowOverlap;
    if (ask) {
      m.since_check = 0;
      const auto [score, box] = m.localize();
      if (score > kMinFaceScore && box.width >= kMinBox && box.height >= kMinBox) {
        m.found = box;
        if (!m.followed || box_overlap(box, *m.followed) < kFollowOverlap)
          m.followed = box;  // first face, or the face moved somewhere else
      }
      else {
        m.found.reset();
        if (!m.followed)
          return {};
        // Unsure, but a face is followed: the pose network's uncertainty decides
      }
    }

    // 2. Head pose from a square crop around the face
    const cv::Rect2f roi = *m.followed;
    const float size = std::max(roi.width, roi.height);
    const cv::Point2f centre(roi.x + roi.width / 2, roi.y + roi.height / 2);
    if (size < kMinBox) {
      reset();
      return {};
    }
    const int side = int(std::lround(size));
    cv::getRectSubPix(m.grey, cv::Size(side, side), centre, m.crop);
    cv::resize(m.crop, m.scaled, m.pose.input_size, 0, 0, cv::INTER_AREA);
    normalize_face_crop(m.scaled, m.tensor);
    auto out = m.pose.run(m.memory, m.tensor);

    // Outputs are relative to the crop: -1 ... 1 from its centre
    const float half = side / 2.f;
    const float* q = out[m.out_quat].GetTensorData<float>();
    const float* b = out[m.out_box].GetTensorData<float>();
    const float* c = out[m.out_centre].GetTensorData<float>();
    const cv::Rect2f box(centre.x + half * b[0], centre.y + half * b[1],
                         half * (b[2] - b[0]), half * (b[3] - b[1]));
    const cv::Point2d face_centre(centre.x + half * c[0], centre.y + half * c[1]);
    const double uncertainty = m.out_uncertainty >= 0
      ? rotation_uncertainty_deg(out[m.out_uncertainty].GetTensorData<float>()) : 0.0;

    const cv::Rect2f picture(0, 0, float(m.grey.cols), float(m.grey.rows));
    if (uncertainty > kMaxUncertainty || box.width < kMinBox || box.height < kMinBox
        || (box & picture).area() <= 0) {
      reset();  // no face in the crop (any more)
      return {};
    }
    m.followed = box;

    const HeadPose pose = head_pose_from_quaternion(cv::Vec4f(q[0], q[1], q[2], q[3]), face_centre, m.grey.size());
    if (!pose.ok) {
      reset();
      return {};
    }
    NeuralFace face;
    face.ok = true;
    face.yaw = pose.yaw;
    face.pitch = pose.pitch;
    face.box = box;
    face.uncertainty = uncertainty;
    return face;
  }
  catch (const std::exception& e) {
    std::cerr << "NeuralNet face model: " << e.what() << std::endl;
    reset();
    return {};
  }
}

#else  // built without ONNX Runtime

struct NeuralPose::Impl
{
};

bool NeuralPose::built_in() { return false; }

NeuralPose::NeuralPose(const std::string&, int)
  : impl_(std::make_unique<Impl>())
{
  std::cerr << "NeuralNet face model: not built in (CMake option RPIHT_NEURALNET, 64-bit ARM only)" << std::endl;
}

NeuralPose::~NeuralPose() = default;
bool NeuralPose::ok() const { return false; }
void NeuralPose::reset() {}
NeuralFace NeuralPose::process(const cv::Mat&) { return {}; }

#endif
