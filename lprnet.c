/*
 * Copyright (C) 2017 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>

#include "lprnet.h"
#include "lprnetKernels.h"
#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#ifdef SILENT
	#define PRINTF(...) ((void) 0)
#else
	#define PRINTF printf
#endif

#ifdef __EMUL__
char *ImageName;
#ifdef PERF
#undef PERF
#endif
#endif

#define AT_INPUT_SIZE (AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR*AT_INPUT_COLORS_LPR)

#ifndef STACK_SIZE
#define STACK_SIZE     2048 
#endif

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;

static char *CHAR_DICT [71] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "<Anhui>", "<Beijing>", "<Chongqing>", "<Fujian>", "<Gansu>", "<Guangdong>", "<Guangxi>", "<Guizhou>", "<Hainan>", "<Hebei>", "<Heilongjiang>", "<Henan>", "<HongKong>", "<Hubei>", "<Hunan>", "<InnerMongolia>", "<Jiangsu>", "<Jiangxi>", "<Jilin>", "<Liaoning>", "<Macau>", "<Ningxia>", "<Qinghai>", "<Shaanxi>", "<Shandong>", "<Shanghai>", "<Shanxi>", "<Sichuan>", "<Tianjin>", "<Tibet>", "<Xinjiang>", "<Yunnan>", "<Zhejiang>", "<police>", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "_"};

signed char Output_1[71*88];
typedef signed char IMAGE_IN_T;
unsigned char Input_1[AT_INPUT_SIZE];

static void RunNetwork()
{
#ifdef PERF
	PRINTF("Start timer\n");
	gap_cl_starttimer();
	gap_cl_resethwtimer();
#endif
	__PREFIX(CNN)(Input_1, Output_1);
	PRINTF("Runner completed\n");
	int max_prob;
	int predicted_char = 70;
	PRINTF("OUTPUT: \n");
	for (int i=0; i<88; i++){
		max_prob = 0x80000000;
		for (int j=0; j<71; j++){
			if (Output_1[i+j*88]>max_prob){
				max_prob = Output_1[i+j*88];
				predicted_char = j;
			}
		}
		if (predicted_char==70) continue;
		PRINTF("%s, ", CHAR_DICT[predicted_char]);
	}
	PRINTF("\n");
}

int start()
{
	PRINTF("Entering main controller\n");
	
#ifndef __EMUL__
	#ifdef MEASUREMENTS
    pi_gpio_pin_configure(NULL, PI_GPIO_A0_PAD_8_A4, PI_GPIO_OUTPUT);
    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
    #endif

	char *ImageName = __XSTR(AT_IMAGE);
	struct pi_device cluster_dev;
	struct pi_cluster_task *task;
	struct pi_cluster_conf conf;

	pi_cluster_conf_init(&conf);
	pi_open_from_conf(&cluster_dev, (void *)&conf);
	pi_cluster_open(&cluster_dev);
	pi_freq_set(PI_FREQ_DOMAIN_CL, FREQ_CL*1000*1000);
	pi_freq_set(PI_FREQ_DOMAIN_FC, FREQ_FC*1000*1000);
	task = pmsis_l2_malloc(sizeof(struct pi_cluster_task));
	if (!task) {
		printf("failed to allocate memory for task\n");
	}
	memset(task, 0, sizeof(struct pi_cluster_task));
	task->entry = &RunNetwork;
	task->stack_size = STACK_SIZE;
	task->slave_stack_size = SLAVE_STACK_SIZE;
	task->arg = NULL;
#endif

	PRINTF("Constructor\n");
	// IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
	if (__PREFIX(CNN_Construct)())
	{
		printf("Graph constructor exited with an error\n");
		return 1;
	}

	PRINTF("Reading image\n");
	//Reading Image from Bridge
	if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH_LPR, AT_INPUT_HEIGHT_LPR, AT_INPUT_COLORS_LPR,
			      		  Input_1, AT_INPUT_SIZE*sizeof(IMAGE_IN_T), IMGIO_OUTPUT_CHAR, 0)) {
		printf("Failed to load image %s\n", ImageName);
		return 1;
	}
	PRINTF("Finished reading image\n");

	PRINTF("Call cluster\n");
#ifndef __EMUL__
	#ifdef MEASUREMENTS
	for (int i=0; i<1000; i++){
		pi_time_wait_us(50000);
	    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 1);
		// Execute the function "RunNetwork" on the cluster.
		pi_cluster_send_task_to_cl(&cluster_dev, task);
	    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
    }
    #else
		// Execute the function "RunNetwork" on the cluster.
		pi_cluster_send_task_to_cl(&cluster_dev, task);
	#endif
#else
	RunNetwork();
#endif

	__PREFIX(CNN_Destruct)();

#ifdef PERF
	{
		unsigned int TotalCycles = 0, TotalOper = 0;
		printf("\n");
		for (int i=0; i<(sizeof(AT_GraphPerf)/sizeof(unsigned int)); i++) {
			printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", AT_GraphNodeNames[i],
			       AT_GraphPerf[i], AT_GraphOperInfosNames[i], ((float) AT_GraphOperInfosNames[i])/ AT_GraphPerf[i]);
			TotalCycles += AT_GraphPerf[i]; TotalOper += AT_GraphOperInfosNames[i];
		}
		printf("\n");
		printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
		printf("\n");
	}
#endif

#ifndef __EMUL__
	pmsis_exit(0);
#endif
	PRINTF("Ended\n");
	return 0;
}

#ifndef __EMUL__
int main(void)
{
    PRINTF("\n\n\t *** NNTOOL LPRNET ***\n\n");
  	return pmsis_kickoff((void *) start);
}
#else
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        PRINTF("Usage: mnist [image_file]\n");
        exit(-1);
    }
    ImageName = argv[1];
    PRINTF("\n\n\t *** NNTOOL LPRNET emul ***\n\n");
    start();
    return 0;
}
#endif
