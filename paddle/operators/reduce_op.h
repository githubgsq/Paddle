/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once

#include "paddle/operators/math/math_function.h"

#include "paddle/framework/eigen.h"
#include "paddle/framework/op_registry.h"

namespace paddle {
namespace operators {

using Tensor = framework::Tensor;
using DDim = framework::DDim;
template <typename T, size_t D, int MajorType = Eigen::RowMajor,
          typename IndexType = Eigen::DenseIndex>
using EigenTensor = framework::EigenTensor<T, D, MajorType, IndexType>;

struct SumFunctor {
  template <typename Place, typename In, typename Out, typename Dim>
  void operator()(const Place& place, In& in, Out& out, const Dim& dim) {
    out.device(place) = in.sum(dim);
  }
};

struct SumGradFunctor {
  template <typename Place, typename In, typename In_Const, typename Out,
            typename Dim>
  void operator()(const Place& place, In_Const& in, In& in_grad, Out& out,
                  Out& out_grad, const Dim& dim, int size) {
    in_grad.device(place) = out_grad.broadcast(dim);
  }
};

struct MeanFunctor {
  template <typename Place, typename In, typename Out, typename Dim>
  void operator()(const Place& place, In& in, Out& out, const Dim& dim) {
    out.device(place) = in.mean(dim);
  }
};

struct MeanGradFunctor {
  template <typename Place, typename In, typename In_Const, typename Out,
            typename Dim>
  void operator()(const Place& place, In_Const& in, In& in_grad, Out& out,
                  Out& out_grad, const Dim& dim, int size) {
    in_grad.device(place) = out_grad.broadcast(dim) / in_grad.constant(size);
  }
};

struct MaxFunctor {
  template <typename Place, typename In, typename Out, typename Dim>
  void operator()(const Place& place, In& in, Out& out, const Dim& dim) {
    out.device(place) = in.maximum(dim);
  }
};

struct MinFunctor {
  template <typename Place, typename In, typename Out, typename Dim>
  void operator()(const Place& place, In& in, Out& out, const Dim& dim) {
    out.device(place) = in.minimum(dim);
  }
};

struct MaxOrMinGradFunctor {
  template <typename Place, typename In, typename In_Const, typename Out,
            typename Dim>
  void operator()(const Place& place, In_Const& in, In& in_grad, Out& out,
                  Out& out_grad, const Dim& dim, int size) {
    auto equals = in == out.broadcast(dim);
    auto ones = in_grad.constant(1);
    auto zeros = in_grad.constant(0);
    in_grad.device(place) =
        out_grad.broadcast(dim) * equals.select(ones, zeros);
  }
};

template <typename Place, typename T, typename Functor>
class ReduceKernel : public framework::OpKernel {
 public:
  void Compute(const framework::ExecutionContext& context) const override {
    int rank = context.Input<Tensor>("X")->dims().size();
    switch (rank) {
      case 1:
        ReduceCompute<1>(context);
        break;
      case 2:
        ReduceCompute<2>(context);
        break;
      case 3:
        ReduceCompute<3>(context);
        break;
      case 4:
        ReduceCompute<4>(context);
        break;
      case 5:
        ReduceCompute<5>(context);
        break;
      case 6:
        ReduceCompute<6>(context);
        break;
    }
  }

 private:
  template <size_t D>
  void ReduceCompute(const framework::ExecutionContext& context) const {
    auto* input = context.Input<Tensor>("X");
    auto* output = context.Output<Tensor>("Out");
    output->mutable_data<T>(context.GetPlace());

    auto x = EigenTensor<T, D>::From(*input);
    auto x_rank = static_cast<int>(x.dimensions().size());
    int dim = static_cast<int>(context.Attr<int>("dim"));
    if (dim < 0) dim = x_rank + dim;
    auto reduce_dim = Eigen::array<int, 1>({{dim}});
    // construct the squeezed output tensor
    bool keep_dim = context.Attr<int>("keep_dim") == 1;
    DDim dims = output->dims();
    auto dims_vector = vectorize(dims);
    if (keep_dim && x_rank > 1) {
      dims_vector.erase(dims_vector.begin() + dim);
      dims = framework::make_ddim(dims_vector);
    }
    auto out = EigenTensor < T, D == 1 ? 1 : (D - 1) > ::From(*output, dims);
    auto& place = context.GetEigenDevice<Place>();
    Functor functor;
    functor(place, x, out, reduce_dim);
  }
};

template <typename Place, typename T, typename Functor>
class ReduceGradKernel : public framework::OpKernel {
 public:
  void Compute(const framework::ExecutionContext& context) const override {
    int rank = context.Input<Tensor>("X")->dims().size();
    switch (rank) {
      case 1:
        ReduceCompute<1>(context);
        break;
      case 2:
        ReduceCompute<2>(context);
        break;
      case 3:
        ReduceCompute<3>(context);
        break;
      case 4:
        ReduceCompute<4>(context);
        break;
      case 5:
        ReduceCompute<5>(context);
        break;
      case 6:
        ReduceCompute<6>(context);
        break;
    }
  }

 private:
  template <size_t D>
  void ReduceCompute(const framework::ExecutionContext& context) const {
    auto* input0 = context.Input<Tensor>("X");
    auto* input1 = context.Input<Tensor>("Out");
    auto* input2 = context.Input<Tensor>(framework::GradVarName("Out"));
    auto* output = context.Output<Tensor>(framework::GradVarName("X"));

    if (output != nullptr) {
      output->mutable_data<T>(context.GetPlace());
      auto x = EigenTensor<T, D>::From(*input0);
      auto x_grad = EigenTensor<T, D>::From(*output);
      auto x_rank = static_cast<int>(x.dimensions().size());
      int dim = static_cast<int>(context.Attr<int>("dim"));
      if (dim < 0) dim = x_rank + dim;
      DDim dims = input0->dims();
      dims[dim] = 1;
      auto x_reduce = EigenTensor<T, D>::From(*input1, dims);
      auto x_reduce_grad = EigenTensor<T, D>::From(*input2, dims);

      Eigen::array<int, D> braodcast_dim;
      for (size_t i = 0; i < D; ++i) braodcast_dim[i] = 1;
      braodcast_dim[dim] = input0->dims()[dim];
      auto& place = context.GetEigenDevice<Place>();
      Functor functor;
      functor(place, x, x_grad, x_reduce, x_reduce_grad, braodcast_dim,
              braodcast_dim[dim]);
    }
  }
};

// For EigenTensor unsupported reduce
template <typename T, typename Functor>
class ReduceGradEigenFreeKernel : public framework::OpKernel {
 public:
  void Compute(const framework::ExecutionContext& context) const override {
    auto* x = context.Input<Tensor>("X");
    auto* out = context.Input<Tensor>("Out");
    auto* x_grad = context.Output<Tensor>(framework::GradVarName("X"));
    auto* out_grad = context.Input<Tensor>(framework::GradVarName("Out"));
    if (x_grad != nullptr) {
      DDim dims = x->dims();
      int rank = dims.size();
      int dim = static_cast<int>(context.Attr<int>("dim"));
      if (dim < 0) dim = rank + dim;

      auto* x_data = x->data<T>();
      auto* x_grad_data = x_grad->mutable_data<T>(context.GetPlace());
      auto* out_data = out->data<T>();
      auto* out_grad_data = out_grad->data<T>();

      int outer_count = 1;
      int inner_count = 1;
      int mid_count = dims[dim];
      for (int i = 0; i < dim; ++i) {
        outer_count *= dims[i];
      }
      for (int i = dim + 1; i < rank; ++i) {
        inner_count *= dims[i];
      }

      int x_offset = 0;    // offset on raw data
      int out_offset = 0;  // offset on reduced data
      Functor functor;
      for (int i = 0; i < outer_count; ++i) {
        for (int j = 0; j < inner_count; ++j) {
          out_offset = inner_count * i + j;
          for (int k = 0; k < mid_count; ++k) {
            x_offset = (inner_count * mid_count) * i + inner_count * k + j;
            functor(x_data + x_offset, x_grad_data + x_offset,
                    out_data + out_offset, out_grad_data + out_offset,
                    mid_count);
          }
        }
      }
    }
  }
};

}  // namespace operators
}  // namespace paddle
