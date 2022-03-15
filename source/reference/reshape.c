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

#include "csi_ref.h"
#include "csi_utils.h"

int csi_ref_reshape_init(struct csi_tensor *input, struct csi_tensor *output,
                         struct reshape_params *params)
{
    if (input->quant_channel == output->quant_channel) {
        int quant_size = input->quant_channel * sizeof(struct csi_quant_info);
        int t = memcmp(input->qinfo, output->qinfo, quant_size);
        if (t == 0) {
            params->base.bc = csi_ref_reshape;
            return CSINN_TRUE;
        }
    }
    params->base.bc = csi_ref_reshape_quant;
    return CSINN_TRUE;
}

int csi_ref_reshape(struct csi_tensor *input, struct csi_tensor *output,
                    struct reshape_params *params)
{
    float *input_data = input->data;
    float *output_data = output->data;
    int size = csi_tensor_byte_size(input);
    if (input_data != output_data) {
        memcpy(output_data, input_data, size);
    }
    return CSINN_TRUE;
}

int csi_ref_reshape_quant(struct csi_tensor *input, struct csi_tensor *output,
                          struct reshape_params *params)
{
    return csi_ref_siso_callback_base(input, output, params, csi_ref_reshape);
}
