/*
 * Copyright (C) 2016-2022 T-Head Semiconductor Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* CSI-NN2 version 1.12.x */

#include "csi_thead_rvv.h"

void csi_nn_rvv_conv1x1s1_gemm_transform_kernel_fp16(struct csi_tensor *kernel,
                                                     struct conv2d_params *params)
{
    __fp16 *kernel_data = (__fp16 *)kernel->data;
    int group = params->group;

    int m = kernel->dim[0] / group;  // out_ch
    int k = kernel->dim[1];          // in_ch ( kernel->dim[2] = kernel->dim[3] = 1)

    __fp16 *pa_reorder = (__fp16 *)csi_mem_alloc(group * m * k * sizeof(__fp16));
    for (int g = 0; g < group; g++) {
        csi_nn_rvv_reorder_kernel_n8_fp16(kernel_data + g * m * k, pa_reorder + g * m * k, m, k, k);
    }
    memcpy(kernel_data, pa_reorder, group * m * k * sizeof(__fp16));
    csi_mem_free(pa_reorder);
}

int csi_nn_rvv_conv1x1s1_gemm_fp16(struct csi_tensor *input, struct csi_tensor *output,
                                   struct csi_tensor *kernel, struct csi_tensor *bias,
                                   struct conv2d_params *params)
{
    __fp16 *input_data = (__fp16 *)input->data;
    __fp16 *output_data = (__fp16 *)output->data;
    __fp16 *kernel_data = (__fp16 *)kernel->data;
    __fp16 *bias_data = (__fp16 *)bias->data;

    int32_t group = params->group;
    int32_t batch = input->dim[0];  // assert(batch == 1);
    int32_t in_ch = input->dim[1];
    int32_t out_ch = kernel->dim[0];
    int32_t out_h = output->dim[2];
    int32_t out_w = output->dim[3];

    int32_t m = out_ch / group;
    int32_t k = in_ch / group;
    int32_t n = out_h * out_w;

    __fp16 *pb_reorder = (__fp16 *)csi_mem_alloc(k * n * sizeof(__fp16));

    for (int i = 0; i < batch; i++) {
        for (int g = 0; g < group; g++) {
            __fp16 *pa = kernel_data + g * m * k;
            __fp16 *pb = pb_reorder;
            __fp16 *pc = output_data;

            // pack
            csi_nn_rvv_reorder_input_z16_fp16(input_data, pb, k, n, n);
            // GEMM
            csi_nn_rvv_gemm_8x16_fp16(pc, pa, pb, m, k, n, n, bias_data + g * m);

            input_data += k * n;
            output_data += m * n;
        }
    }
    csi_mem_free(pb_reorder);
    return CSINN_TRUE;
}
