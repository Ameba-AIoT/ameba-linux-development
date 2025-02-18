/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <cstdio>

#include "models/keras_mnist_conv_model_data.h"
#include "testdata/input_int8_test_data.h"
#include "testdata/label_int8_test_data.h"

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"

#define TFLITE_MINIMAL_CHECK(x)                                                \
    if (!(x)) {                                                                \
        fprintf(stderr, "Error at %s:%d\n", __FILE__, __LINE__);               \
        exit(1);                                                               \
    }

long long get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

TfLiteStatus LoadQuantModelAndPerformInference() {

    // Load model
    std::unique_ptr<tflite::FlatBufferModel> model =
        tflite::FlatBufferModel::BuildFromBuffer(
            (const char *)g_keras_mnist_conv_model_data,
            g_keras_mnist_conv_model_data_size);

    TFLITE_MINIMAL_CHECK(model != nullptr);

    // Build the interpreter with the InterpreterBuilder.
    // Note: all Interpreters should be built with the InterpreterBuilder,
    // which allocates memory for the Interpreter and does various set up
    // tasks so that the Interpreter can read the provided model.
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder builder(*model, resolver);
    std::unique_ptr<tflite::Interpreter> interpreter;
    builder(&interpreter);
    TFLITE_MINIMAL_CHECK(interpreter != nullptr);

    // Allocate tensor buffers.
    TFLITE_MINIMAL_CHECK(interpreter->AllocateTensors() == kTfLiteOk);

    TfLiteTensor *input = interpreter->tensor(interpreter->inputs()[0]);
    TfLiteTensor *output = interpreter->tensor(interpreter->outputs()[0]);

    int correct = 0;
    const int kCategoryCount = 10;
    const int input_size =
        g_input_int8_test_data_size / g_label_int8_test_data_size;
    float total_time_ms = 0;
    long long t1 = get_current_time_ms();
    int prediction_index = 0;
    for (unsigned int i = 0; i < g_label_int8_test_data_size; i++) {
        for (int k = 0; k < input_size; k++) {
            input->data.int8[k] = g_input_int8_test_data[i * input_size + k];
        }
        TFLITE_MINIMAL_CHECK(interpreter->Invoke() == kTfLiteOk);
        int max_value = -128;
        for (int j = 0; j < kCategoryCount; ++j) {
            if (output->data.int8[j] > max_value) {
                max_value = output->data.int8[j];
                prediction_index = j;
            }
        }
        if (prediction_index == g_label_int8_test_data[i]) {
            correct += 1;
        }
    }
    long long t2 = get_current_time_ms();
    total_time_ms = t2 - t1;

    printf("[TFLITE-MNIST] Post-invoke Interpreter State\n");
    tflite::PrintInterpreterState(interpreter.get());

    printf("\n\n[TFLITE-MNIST] Accuracy: %d/%d\n", correct, g_label_int8_test_data_size);
    printf("[TFLITE-MNIST] Total Time: %f ms\n\n", total_time_ms);

    return kTfLiteOk;
}

int main(int argc, char *argv[]) {

    TFLITE_MINIMAL_CHECK(LoadQuantModelAndPerformInference() == kTfLiteOk);
    printf("[TFLITE-MNIST] Test Completed!\n");

    return 0;
}
