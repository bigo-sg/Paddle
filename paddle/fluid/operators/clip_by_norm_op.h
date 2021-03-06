/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

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

#include "paddle/fluid/framework/eigen.h"
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/framework/selected_rows.h"
#include "paddle/fluid/operators/math/selected_rows_functor.h"
#include "paddle/fluid/platform/transform.h"

namespace paddle {
namespace operators {

using Tensor = framework::Tensor;
using SelectedRows = framework::SelectedRows;
template <typename T, int MajorType = Eigen::RowMajor,
          typename IndexType = Eigen::DenseIndex>
using EigenVector = framework::EigenVector<T, MajorType, IndexType>;

template <typename DeviceContext, typename T>
class ClipByNormKernel : public framework::OpKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& context) const override {
    auto max_norm = context.Attr<T>("max_norm");
    auto in_var = context.InputVar("X");

    Tensor* output = nullptr;
    const Tensor* input = nullptr;
    if (in_var->IsType<framework::LoDTensor>()) {
      input = context.Input<Tensor>("X");

      output = context.Output<Tensor>("Out");
      output->mutable_data<T>(context.GetPlace());
    } else if (in_var->IsType<SelectedRows>()) {
      auto* x = context.Input<SelectedRows>("X");

      // merge ids in selected rows first
      math::scatter::MergeAdd<DeviceContext, T> merge_func;
      SelectedRows* merged_input =
          const_cast<framework::Scope&>(context.scope())
              .Var()
              ->GetMutable<SelectedRows>();
      merge_func(context.template device_context<DeviceContext>(), *x,
                 merged_input);
      input = &(merged_input->value());

      SelectedRows* output_selected_rows = context.Output<SelectedRows>("Out");
      output_selected_rows->set_rows(merged_input->rows());
      output_selected_rows->set_height(merged_input->height());
      output = output_selected_rows->mutable_value();
      output->Resize(merged_input->value().dims());
      output->mutable_data<T>(context.GetPlace());
    } else {
      PADDLE_THROW("Unexpected branch, input variable type is %s",
                   in_var->Type().name());
    }

    PADDLE_ENFORCE_NOT_NULL(input);

    auto x = EigenVector<T>::Flatten(*input);
    auto out = EigenVector<T>::Flatten(*output);
    auto x_norm = x.square().sum().sqrt();
    auto& place =
        *context.template device_context<DeviceContext>().eigen_device();

    auto temp = (x_norm <= max_norm).template cast<T>().eval();
    auto scaling = temp + (static_cast<T>(1) - temp) * max_norm / x_norm;
    Eigen::array<int, 1> one_dim{{1}};
    Eigen::DSizes<int, 1> m_dsize(input->numel());
    out.device(place) = x * scaling.reshape(one_dim).broadcast(m_dsize);
  }
};

}  // namespace operators
}  // namespace paddle
