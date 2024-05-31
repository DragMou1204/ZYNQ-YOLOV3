/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */
#include <stdio.h>
#include "xil_printf.h"


#include "math.h"
#include "sleep.h"
#include "xscugic.h"
#include "xil_types.h"
#include "xparameters.h"
#include "xil_cache.h"
#include "xtime_l.h"
#include "ff.h"
#include "accel_data.h"
#include "accel_parameter.h"
#include "accel.h"
#include "xil_exception.h"
#include "xplatform_info.h"
#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "display_demo.h"
#include "display_ctrl/display_ctrl.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xiicps.h"
#include "vdma.h"
#include "i2c/PS_i2c.h"
#include "xgpiops.h"
#include "sleep.h"
#include "ov5640.h"
#include "xhls_rect.h"
#include "xhls_preprocess.h"
#include "config.h"
#include "demosaic.h"
#include "gamma_lut.h"
typedef u64 XTime;

#define DEMO_MAX_FRAME (1920*1080*3)
#define DEMO_STRIDE (1280 * 3)
//#define DEMO_MAX_FRAME		(720*1280) //�����yolo�Ķ���
#define RESIZE_IMG_SIZE		(418*258)
u8	ifmInBuf[3][RESIZE_IMG_SIZE*8]	__attribute((aligned (64)));//��������ͼ�Ĵ洢����
XTime timeEnd,timeCur;//��ʼ��ʱ��
uint32_t timeUsed;//

//const uint8_t color1[MAX_CLASS]={255,0,0};
//const uint8_t color2[MAX_CLASS]={0,0,255};
//const uint8_t color3[MAX_CLASS]={0,255,0};
//FIL			fil; //�ļ�ϵͳ����
//FATFS		fatfs;//ָ���ļ�ϵͳ�����ָ��
//FRESULT		response;
//uint32_t	wr_tot;

char model_id[3]={'F','H','A'};
uint8_t model_class[3]={3,2,2};
uint8_t model_num=2;
uint8_t model_index=2;
uint8_t change_model_flag=0;
uint8_t ofmbuff[]={4,5,6};

XScuGic				INTCInst;
u8 ifmbuff[RESIZE_IMG_SIZE*8]  __attribute((aligned (64)));
#define INTC_DEVICE_INT_ID	XPS_FPGA0_INT_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID// change 0U
void Setup_File_System();
int Setup_Intr_System(u16 DeviceId);
void Conv_Accel_Callback(void *CallbackRef);
void hls_draw_initialize();
void display_initialize();
void Setup_Accel_System();
void preprocess_initialize();
void hls_draw(float conf);
//display define
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_1_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
//#define VID_VTC_IRPT_ID XPS_FPGA3_INT_ID
//#define VID_GPIO_IRPT_ID XPS_FPGA4_INT_ID
#define SCU_TIMER_ID XPAR_SCUTIMER_DEVICE_ID

#define BLANK 26
#define CAM_EMIO	78
/*
 * Display Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma vdma;
XIicPs ps_i2c0;
XIicPs ps_i2c1;
//XGpio hdmi_rstn;
//XGpio cmos_rstn;

XGpioPs Gpio;
/*
 * Framebuffers for video data
 */
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME] __attribute__ ((aligned(64)));
u8 *pFrames[DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers

/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
/* ------------------------------------------------------------ */
void adv7511_init(XIicPs *IicInstance) ;
//
XHls_preprocess		hlsPreprocess;
XHls_rect			hlsRect;
#define VDMA_BASEADDR0 XPAR_AXI_VDMA_2_BASEADDR
int main()
{
	xil_printf("Enter Main!\n\r");
    Xil_DCacheEnable();//ʹ��DCACHE
    Setup_File_System();//�����ļ�ϵͳ
   // Load_pic();//����ͼƬ
    Setup_Intr_System(INTC_DEVICE_ID);
    Setup_Accel_System();//DMA��ʼ��
    Load_Para(model_id[model_index],model_class[model_index]);//����ģ�Ͳ���

//    xil_printf("[start %d end %d]\n\r",timeCur,timeEnd);
//    xil_printf("[Time elapsed is %d us %d second]\n\r",timeUsed,(timeEnd-timeCur)/(COUNTS_PER_SECOND));//�����ʱ
    preprocess_initialize();//ͼ��Ԥ����ü�IP�˳�ʼ��
    hls_draw_initialize();
    display_initialize();
   // hls_draw();

    XTime_GetTime(&timeCur);

    YOLOV3_TINY();

    XTime_GetTime(&timeEnd);
    timeUsed=((timeEnd-timeCur)*1000000)/(COUNTS_PER_SECOND);
    xil_printf("[Time elapsed is %d us %d second]\n\r",timeUsed,(timeEnd-timeCur)/(COUNTS_PER_SECOND));//�����ʱ
    hls_draw(0.6);
    while(1){
    		XTime_GetTime(&timeCur);
    		YOLOV3_TINY();
    		XTime_GetTime(&timeEnd);
    		timeUsed=((timeEnd-timeCur)*1000000)/(COUNTS_PER_SECOND);
    		xil_printf("[Time elapsed is %d us %d second]\n\r",timeUsed,(timeEnd-timeCur)/(COUNTS_PER_SECOND));//�����ʱ
    		hls_draw(0.6);
    		}

    return 0;
}

void preprocess_initialize(){
	XHls_preprocess_Initialize(&hlsPreprocess,XPAR_HLS_PREPROCESS_0_DEVICE_ID);
	XHls_preprocess_EnableAutoRestart(&hlsPreprocess);
	XHls_preprocess_InterruptGlobalDisable(&hlsPreprocess);
	XHls_preprocess_Start(&hlsPreprocess);
	Xil_Out32((VDMA_BASEADDR0 + 0x030), 0x108B);				// enable circular mode ѭ���洢����̬ʱ�������ڲ�ʱ��   ��DDRд�������ã�S2MM��
	Xil_Out32((VDMA_BASEADDR0 + 0x0AC), (UINTPTR)ifmInBuf[0]);	// start address VMDA�洢��ַ
	Xil_Out32((VDMA_BASEADDR0 + 0x0B0), (UINTPTR)ifmInBuf[1]);	// start address
	Xil_Out32((VDMA_BASEADDR0 + 0x0B4), (UINTPTR)ifmInBuf[2]);	// start address

	Xil_Out32((VDMA_BASEADDR0 + 0x0A8), (418*8));				// h offset (H_STRIDE* 4) bytes
	Xil_Out32((VDMA_BASEADDR0 + 0x0A4), (418*8));				// h size (H_ACTIVE * 4) bytes
	Xil_Out32((VDMA_BASEADDR0 + 0x0A0), 258);			// v size (V_ACTIVE)

	xil_printf("Video Resize System Setup Success!\n");
	return;
}
void hls_draw_initialize(){
			XHls_rect_Initialize(&hlsRect,XPAR_HLS_RECT_0_DEVICE_ID);//HLS����IP�˵ĳ�ʼ��
	    	XHls_rect_EnableAutoRestart(&hlsRect);//ʹ���Զ�����
	    	XHls_rect_InterruptGlobalDisable(&hlsRect);//�ر�ȫ���ж�
	    	XHls_rect_Start(&hlsRect);//����
	    	//��ʼ��Ԥ�����ַ�
	    	XHls_rect_Set_xleft_s(&hlsRect,0);
	    	XHls_rect_Set_xright_s(&hlsRect,0);
	    	XHls_rect_Set_ytop_s(&hlsRect,0);
	    	XHls_rect_Set_ydown_s(&hlsRect,0);
	    	XHls_rect_Set_char1(&hlsRect,BLANK);
	    	XHls_rect_Set_char2(&hlsRect,BLANK);
	    	XHls_rect_Set_char3(&hlsRect,BLANK);
	    	XHls_rect_Set_char4(&hlsRect,BLANK);
	    	XHls_rect_Set_char5(&hlsRect,BLANK);
	    	XHls_rect_Set_char6(&hlsRect,BLANK);
	    	xil_printf("hls draw ip core start\r\n");
	    	return;
}
void hls_draw(float conf){
	if(prob>conf){
			XHls_rect_Set_color1(&hlsRect,color1[category]);
			XHls_rect_Set_color2(&hlsRect,color2[category]);
			XHls_rect_Set_color3(&hlsRect,color3[category]);
			XHls_rect_Set_xleft_s(&hlsRect,xleft);
			XHls_rect_Set_xright_s(&hlsRect,xright);
			XHls_rect_Set_ytop_s(&hlsRect,ytop);
			XHls_rect_Set_ydown_s(&hlsRect,ydown);
			XHls_rect_Set_char1(&hlsRect,cate[category][0]-'a');
			XHls_rect_Set_char2(&hlsRect,cate[category][1]-'a');
			XHls_rect_Set_char3(&hlsRect,cate[category][2]-'a');
			XHls_rect_Set_char4(&hlsRect,cate[category][3]-'a');
			XHls_rect_Set_char5(&hlsRect,cate[category][4]-'a');
			//XHls_rect_Set_char6(&hlsRect,cate[category][5]-'a');
		}
		else{
			XHls_rect_Set_xleft_s(&hlsRect,0);
			XHls_rect_Set_xright_s(&hlsRect,0);
			XHls_rect_Set_ytop_s(&hlsRect,0);
			XHls_rect_Set_ydown_s(&hlsRect,0);
			XHls_rect_Set_char1(&hlsRect,BLANK);
			XHls_rect_Set_char2(&hlsRect,BLANK);
			XHls_rect_Set_char3(&hlsRect,BLANK);
			XHls_rect_Set_char4(&hlsRect,BLANK);
			XHls_rect_Set_char5(&hlsRect,BLANK);
			XHls_rect_Set_char6(&hlsRect,BLANK);
		}
	//xil_printf("hls_draw finish \r\n");
		return;
}
void display_initialize(){
			int Status;
	    	XAxiVdma_Config *vdmaConfig;
	    	XGpioPs_Config *GPIO_CONFIG ;
	    	int i;
	    	/*
	    	  * Initialize an array of pointers to the 3 frame buffers
	        */
	    	for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
	    		{
	    			pFrames[i] = frameBuf[i];
	    			memset(pFrames[i], 0, DEMO_MAX_FRAME);
	    			Xil_DCacheFlushRange((INTPTR) pFrames[i], DEMO_MAX_FRAME) ;
	    		}
	    		i2c_init(&ps_i2c0, XPAR_XIICPS_0_DEVICE_ID,100000); //I2Cʵ����I2C�豸�ţ�ʱ��Ƶ��
	    		i2c_init(&ps_i2c1, XPAR_XIICPS_1_DEVICE_ID,100000); //I2Cʵ����I2C�豸�ţ�ʱ��Ƶ��

	    		GPIO_CONFIG = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID) ;
	    		XGpioPs_CfgInitialize(&Gpio, GPIO_CONFIG, GPIO_CONFIG->BaseAddr) ;

	    		XGpioPs_SetDirectionPin(&Gpio, CAM_EMIO, 1) ;
	    		XGpioPs_SetOutputEnablePin(&Gpio, CAM_EMIO, 1);
	    		XGpioPs_WritePin(&Gpio, CAM_EMIO, 0) ;
	    		usleep(1000000);
	    		XGpioPs_WritePin(&Gpio, CAM_EMIO, 1) ;
	    		usleep(1000000);

	    		adv7511_init(&ps_i2c1) ;

	    		/* Initialize VDMA driver*/
	    		vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	    		if (!vdmaConfig)
	    		{
	    			xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
	    		}
	    		Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	    		if (Status != XST_SUCCESS)
	    		{
	    			xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
	    		}
	    		/*
	    		 * Initialize the Display controller and start it
	    		 */
	    		Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR,pFrames, DEMO_STRIDE);
	    		if (Status != XST_SUCCESS)
	    		{
	    			xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
	    		}
	    		Status = DisplayStart(&dispCtrl);
	    		if (Status != XST_SUCCESS)
	    		{
	    			xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
	    		}
	    		/* Clear frame buffer */
	    		memset(dispCtrl.framePtr[dispCtrl.curFrame], 0, 1920 * 1080 * 3);
	    		gamma_lut_init();
	    		demosaic_init();
	    		/* Start Sensor Vdma */
	    		vdma_write_init(XPAR_AXIVDMA_0_DEVICE_ID,1280 * 3,720,1280 * 3,(unsigned int)dispCtrl.framePtr[dispCtrl.curFrame]);
	    		sensor_init(&ps_i2c0);
	    		xil_printf("start display\r\n");
}

void Load_pic(){ //��ȡ��������ͼ "1:/000.bin"
	xil_printf("Begin Load_image!\n");
	f_open(&fil,"1:/1.bin",FA_OPEN_EXISTING|FA_READ);
	f_read(&fil,ifmInBuf[1],RESIZE_IMG_SIZE*8,&wr_tot);//�ļ�ϵͳָ�룻�ڴ��ַ����ȡ���ȣ�ʵ�ʶ�ȡ����
	xil_printf("Finish read! read length is %d \n\r",wr_tot);
	f_close(&fil);
	Xil_DCacheFlushRange((UINTPTR)ifmInBuf[1],RESIZE_IMG_SIZE*8);
	for(int i =0;i<32;i++)
		xil_printf("value %d is %d \n\r",i,ifmInBuf[1][i]);
	xil_printf("Finish Load image!\n\r");
	return;
}
DIR dir;
void Setup_File_System(){
	response=f_mount(&fatfs, "1:/", 0);   //0:
	if(response!=FR_OK) xil_printf("File System mount fail!\n\r");
	else xil_printf("File System mount Success!\n\r");



		    //xil_printf("Files in %s:\n", "1:/");

		    /*
	 FATFS fs;
	    FILINFO fileInfo;
	    DIR dir;
	    FRESULT res;
#define DRIVE_PATH "1:/"
	    // �����ļ�ϵͳ
	    res = f_mount(&fs, DRIVE_PATH, 0);
	    if (res != FR_OK) {
	        xil_printf("Failed to mount filesystem\n");

	    }
	    xil_printf("1\n");
	    // ��Ŀ¼
	    res = f_opendir(&dir, DRIVE_PATH);
	    if (res != FR_OK) {
	        xil_printf("Failed to open directory\n");

	    }
	    xil_printf("2\n");
	    // ��ȡĿ¼�е��ļ��б�
	    xil_printf("Files in %s:\n", DRIVE_PATH);
	    while (1) {
	        res = f_readdir(&dir, &fileInfo);
	        if (res != FR_OK || fileInfo.fname[0] == 0) break;  // ��ȡ��ɻ��߷�������
	        xil_printf("- %s\n", fileInfo.fname);
	    }

	    // �ر�Ŀ¼
	    f_closedir(&dir);

	    */
}
void Setup_Accel_System(){

	Xil_Out32(X_AXI_DMA_SEND_RST_ADDR,0x04);//DMA MM2S���Ϳ����û�ͨģʽ
	Xil_Out32(X_AXI_DMA_RECV_RST_ADDR,0x04);//DMA S2MM���տ����û�ͨģʽ
	usleep(1000000);
	Xil_Out32(X_AXI_DMA_SEND_ADDR_1,DMA_READREG_MASK); //0x10003 1bit����DMA 0bit������ 16bit�����ж���ֵΪ1 ����counterΪ1
	Xil_Out32(X_AXI_DMA_RECV_ADDR_1,DMA_READREG_MASK);//0x10003 ͬ�ϣ�����S2MM
	xil_printf("ACCEL System Setup Success!\n\r");
	return;
}
void Conv_Accel_Callback(void *CallbackRef){
	//printf("Enter interrupt\t\n");
	ap_done=1;
	return;
}
int Setup_Intr_System(u16 DeviceId){

		XScuGic_Config *IntcConfig;
			int Status ;


			//check device id
			IntcConfig = XScuGic_LookupConfig(DeviceId);
			//intialization
			Status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress) ;
			if (Status != XST_SUCCESS)
				return XST_FAILURE ;

			XScuGic_SetPriorityTriggerType(&INTCInst, INTC_DEVICE_INT_ID,
					0xA0, 0x3);

			Status = XScuGic_Connect(&INTCInst, INTC_DEVICE_INT_ID,
					(Xil_ExceptionHandler)Conv_Accel_Callback,
					(void *)NULL);//NULL
			if (Status != XST_SUCCESS)
				return XST_FAILURE ;
			XScuGic_Enable(&INTCInst, INTC_DEVICE_INT_ID) ;

			// Xil_ExceptionInit();
			Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
					(Xil_ExceptionHandler)XScuGic_InterruptHandler,
					&INTCInst);
			Xil_ExceptionEnable();
			//Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
			xil_printf("Interrupt System Setup Success!\n");

			return XST_SUCCESS ;
}
