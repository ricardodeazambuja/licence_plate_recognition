/*
 * Copyright (C) 2020 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>

#include "ssdlite_v2_quant_ocr_nntool.h"
#include "ssdlite_v2_quant_ocr_nntoolKernels.h"
#include "ImgIO.h"
#include "ssdlite_v2_quant_ocr_nntoolInfo.h"
//#include "SSDKernels.h"
//#include "BB_utils.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define FIX2FP(Val, Precision)    ((float) (Val) / (float) (1<<(Precision)))

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)

#ifndef STACK_SIZE
#define STACK_SIZE     2048
#endif

#define CL_FREQ 175*1000*1000
#define FC_FREQ 250*1000*1000

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;

#ifdef __EMUL__
char *ImageName;
#else
  #ifdef PERF
  L2_MEM rt_perf_t *cluster_perf;
  #endif
#endif
typedef signed char IMAGE_IN_T;
L2_MEM IMAGE_IN_T *Input_1;

L2_MEM char *Output_1;
L2_MEM char *Output_2;
L2_MEM char *Output_3;
L2_MEM char *Output_4;
L2_MEM char *Output_5;
L2_MEM char *Output_6;
L2_MEM char *Output_7;
L2_MEM char *Output_8;
L2_MEM char *Output_9;
L2_MEM char *Output_10;
L2_MEM char *Output_11;
L2_MEM char *Output_12;


static void RunNetwork()
{
  printf("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
  __PREFIX(CNN)(Input_1, Output_1, Output_2, Output_3, Output_4, Output_5, Output_6, Output_7, Output_8, Output_9, Output_10, Output_11, Output_12);
  printf("Runner completed\n");

  printf("\n");
}

int start()
{
  #ifndef __EMUL__
  char *ImageName = __XSTR(AT_IMAGE);
  struct pi_device cluster_dev;
  struct pi_cluster_task *task;
  struct pi_cluster_conf conf;
  printf("Entering main controller\n");
  pi_cluster_conf_init(&conf);
  pi_open_from_conf(&cluster_dev, (void *)&conf);
  pi_cluster_open(&cluster_dev);
  pi_freq_set(PI_FREQ_DOMAIN_CL,CL_FREQ);
  pi_freq_set(PI_FREQ_DOMAIN_FC,FC_FREQ);
  task = AT_L2_ALLOC(0, sizeof(struct pi_cluster_task));
  memset(task, 0, sizeof(struct pi_cluster_task));
  task->entry = &RunNetwork;
  task->stack_size = STACK_SIZE;
  task->slave_stack_size = SLAVE_STACK_SIZE;
  task->arg = NULL;
  #endif


  printf("Constructor\n");
  //Allocate output buffers:
  Output_1  = (unsigned char*)AT_L2_ALLOC(0, 900*4* sizeof(unsigned char));
  Output_2  = (unsigned char*)AT_L2_ALLOC(0, 480*4* sizeof(unsigned char));
  Output_3  = (unsigned char*)AT_L2_ALLOC(0, 120*4* sizeof(unsigned char));
  Output_4  = (unsigned char*)AT_L2_ALLOC(0, 36 *4* sizeof(unsigned char));
  Output_5  = (unsigned char*)AT_L2_ALLOC(0, 12 *4* sizeof(unsigned char));
  Output_6  = (unsigned char*)AT_L2_ALLOC(0, 6  *4* sizeof(unsigned char));
  Output_7  = (unsigned char*)AT_L2_ALLOC(0, 900*2* sizeof(unsigned char));
  Output_8  = (unsigned char*)AT_L2_ALLOC(0, 480*2* sizeof(unsigned char));
  Output_9  = (unsigned char*)AT_L2_ALLOC(0, 120*2* sizeof(unsigned char));
  Output_10 = (unsigned char*)AT_L2_ALLOC(0, 36 *2* sizeof(unsigned char));
  Output_11 = (unsigned char*)AT_L2_ALLOC(0, 12 *2* sizeof(unsigned char));
  Output_12 = (unsigned char*)AT_L2_ALLOC(0, 6  *2* sizeof(unsigned char));

  if(Output_1==NULL||Output_2==NULL||Output_3==NULL||Output_4==NULL||Output_5==NULL||Output_6==NULL||Output_7==NULL||Output_8==NULL||Output_9==NULL||Output_10==NULL||Output_11==NULL||Output_12==NULL){
    printf("Error Allocating CNN output buffers");
    return 1;
  }

  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }

  printf("Reading image\n");
  //Reading Image from Bridge
  if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, AT_INPUT_COLORS, Input_1, AT_INPUT_WIDTH*AT_INPUT_HEIGHT*sizeof(unsigned short), IMGIO_OUTPUT_RGB565, 0)) {
    printf("Failed to load image %s\n", ImageName);
    return 1;
  }
  printf("Finished reading image\n");

  printf("Call cluster\n");
#ifndef __EMUL__
  // Execute the function "RunNetwork" on the cluster.
  pi_cluster_send_task_to_cl(&cluster_dev, task);
#else
  RunNetwork();
#endif

  __PREFIX(CNN_Destruct)();

#ifndef __EMUL__
  #ifdef PERF
	{
		unsigned int TotalCycles = 0, TotalOper = 0;
		printf("\n");
		for (int i=0; i<(sizeof(AT_GraphPerf)/sizeof(unsigned int)); i++) {
			printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", AT_GraphNodeNames[i], AT_GraphPerf[i], AT_GraphOperInfosNames[i], ((float) AT_GraphOperInfosNames[i])/ AT_GraphPerf[i]);
			TotalCycles += AT_GraphPerf[i]; TotalOper += AT_GraphOperInfosNames[i];
		}
		printf("\n");
		printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
		printf("\n");
	}
  printf("%45s Cycles: %10d\n");
  #endif
  pmsis_exit(0);
#endif

  printf("Ended\n");
  return 0;
}

#ifndef __EMUL__
int main(void)
{
  return pmsis_kickoff((void *) start);
}
#else
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: mnist [image_file]\n");
        exit(-1);
    }
    ImageName = argv[1];
    printf("\n\n\t *** NNTOOL emul Example ***\n\n");
    start();
    return 0;
}
#endif
