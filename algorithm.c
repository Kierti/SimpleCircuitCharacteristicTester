/*
 * algorithm.c
 *
 *  Created on: Jun 24, 2022
 *      Author: ��־��
 */

#include "algorithm.h"
#include "base.h"
#include "math.h"
#include "stdbool.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "arm_common_tables.h"
#include "ADS8688.h"
#include "AD9959.h"
#include "cmd_process.h"
#include "hmi_user_uart.h"
#include "main.h"
#include "tim.h"


Sys sys;		//ϵͳ�ṹ��
Sys sysError;   //���ϲ��Խṹ��

const u8 *CiucuitErrors[15]={
"��·����","R1��·","R1��·","R2��·","R2��·",
"R3��·","R3��·","R4��·","R4��·","C1��·",
"C1�ӱ�","C2��·","C2�ӱ�","C3��·","C3�ӱ�"
};

/**
 * ADS8688��ͨ��AD����
 */
ArrayParam AD_Params;
float AD_array[6][AD_Size]={0,},	//AD������ά����
		*pAD_array,*pAD_array_end;

u8
TFT_array[AD_Size]={0,},
*pTFT_array,*pTFT_array_end
;

#define adResIn1 AD_array[0]	//���������1
#define adResIn2 AD_array[1]	//���������2
#define adResLoad AD_array[2]	//�������
#define adResNoLoad AD_array[3]	//������ؽ���
#define adResRMS AD_array[4]	//�����Чֵ
#define adResDC AD_array[5]		//���ֱ��

/**
 * @brief AD������ά�����ʼ��
 */
void AD_arrInit(void){
	pAD_array=AD_array[0];
	pAD_array_end=AD_array[0]+AD_Size;
	arm_fill_f32(0,AD_array[0],AD_Size);
	arm_fill_f32(0,AD_array[1],AD_Size);
	arm_fill_f32(0,AD_array[2],AD_Size);
	arm_fill_f32(0,AD_array[3],AD_Size);
	arm_fill_f32(0,AD_array[4],AD_Size);
	arm_fill_f32(0,AD_array[5],AD_Size);
}


/**
 * @brief ADS��ֵ�����ö�ʱ���ж�
 */
void getADResults(void)
{
	AD_arrInit();
    get_ADS_allch(pAD_array);
    HAL_TIM_Base_Start_IT(&htim3);
    while(pAD_array!=pAD_array_end){};
    HAL_TIM_Base_Stop_IT(&htim3);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim == &htim3)
    {
        if(pAD_array < pAD_array_end)
        {
            get_ADS_allch(pAD_array++);
        }
    }
}

/**
 * @brief ������ʼ��
 */
void Params_Init(void)
{
	AD_arrInit();
    sys.mode = BaseTest;
    sys.AmpFreqFlag =0;	//�Ƿ���з�Ƶ��������
    sys.inputRes = 0;   //�������
    sys.outputRes= 0;   //�������
    sys.gain     = 0;   //��·����

    sys.upFreq   = 0;   //����Ƶ��
    sys.dB       = 3;   //��Ӧ�ֱ���

    sys.bugElec  = 0;   //����Ԫ��

    sys.Rs       = 6550.340f;//��������ѹ����
    sys.Ro       = 997.870f;//��������ѹ����
    sys.DC       = 0;
    sys.AC       = 0;

    sys.dis      = 0;
    sys.Auto     = 0;
    sys.err      = 0;
    sys.R1R2Short = 0;
    sys.R3R4Open = 0;
    sys.flag = 0 ;
    relayNoLoad;         //����͵�ƽ����
}

/**
 * @brief ����Ҷ�任
 */
#define FFT_SIZE AD_Size
static float FFT_Buffer[FFT_SIZE];
float fftResult[6][AD_Size]={0,},
inputVol[2]={0,},		//����������λ
outputVol[2]={0,},		//���������λ
sweepFreArr[21],		//ɨƵ
ampValue[3],			//�ڵ�ǰ����£��������Ƶ���µķ�ֵ,1kHz,100Hz,100kHz
goodAmp[3]={360,95,1080};			//��·��������£�����Ƶ��������Ĳ���RMS
u32 outFreq[3]={1000,100,100000};	//���Ƶ��
void calc_FFT(float*Input,float*Output)
{
    arm_rfft_fast_instance_f32 S;//�ṹ��
    arm_rfft_fast_init_f32(&S,FFT_SIZE);//��ʼ���ýṹ��
    arm_rfft_fast_f32(&S, Input, FFT_Buffer, 0);//ifft_flag=0�����任�� Ϊ1������任
    arm_cmplx_mag_f32(FFT_Buffer, Output, FFT_SIZE);
    arm_scale_f32(Output,2.0f/FFT_SIZE,Output,FFT_SIZE);    //�������ʵV
    Output[0] *= 0.5f;
}

float32_t Phase_f32[FFT_SIZE]; /* ��λ*/
float fftPhase[6][FFT_SIZE]={0,},
phaseTwo[2],		  //���������λ
skewing,			  //��λ��
judgeC1,
RmsForC1,
judgeC1Rms;
/*
*	�� �� ��: PowerPhaseRadians_f32
*	����˵��: ����λ
*	��    �Σ�_ptr  ��λ��ַ����ʵ�����鲿
*             _phase �����λ����λ�Ƕ��ƣ���Χ(-180, 180]
*             _usFFTPoints  ����������ÿ������������float32_t��ֵ
*             _uiCmpValue  �Ƚ�ֵ����Ҫ�����λ����ֵ
*	�� �� ֵ: ��
*/
void PowerPhaseRadians_f32(float32_t *_ptr, float32_t *_phase, uint16_t _usFFTPoints, float32_t _uiCmpValue)
{
	float32_t lX, lY;
	uint16_t i;
	float32_t phase;
	float32_t mag;

	for (i=0; i <_usFFTPoints; i++)
	{
		lX= _ptr[2*i];  	  /* ʵ�� */
		lY= _ptr[2*i + 1];    /* �鲿 */

 		phase = atan2f(lY, lX);    		  				 /* atan2���Ľ����Χ��(-pi, pi], ������ */
		arm_sqrt_f32((float32_t)(lX*lX+ lY*lY), &mag);   /* ��ģ */

//		if(_uiCmpValue < mag)
//		{
//			_phase[i] = 0;
//		}
//		else
//		{
			_phase[i] = phase* 180.0f/3.1415926f;   /* �����Ľ���ɻ���ת��Ϊ�Ƕ� */
//		}
	}
}

void cal_fftPhase(float*Input,float*Output){
	arm_rfft_fast_instance_f32 S;
	arm_rfft_fast_init_f32(&S,FFT_SIZE);	//��ʼ���ýṹ��
	arm_rfft_fast_f32(&S,Input,FFT_Buffer,0);//ifft_flag=0�����任�� Ϊ1������任
	PowerPhaseRadians_f32(FFT_Buffer,Output,FFT_SIZE,500);
}


/**
 * @brief �жϲ����Ƿ�ʧ��
 */
#define WAVE_THERSHOLD 0.10f
bool isGoodWave(void){
    float sum=0,noise=0,harmonic;u16 i=0;
    getADResults();				//�ɼ���ѹ
    calc_FFT(adResNoLoad,fftResult[3]);	//�������������£��ɼ��ĵ�ѹ������Ҷ�任
    if(fftResult[3][50]<1)		//һ��г����ֵ
	{
    	sinfrq.amp=500;
    	ad9959_write_amplitude(AD9959_CHANNEL_0, 500);
		  return true;
	}
    for(i=1;i<128;i++)
        sum += fftResult[3][i];

    harmonic = fftResult[3][50*2]+fftResult[3][50*3]+fftResult[3][50*4];//г��
    noise = (sum - fftResult[3][50*1]- harmonic)/128;	//����

    if(fftResult[3][50]==noise) noise--;
    	sys.dis = (harmonic-noise) / (fftResult[3][50]-noise);
    SetTFTText(0,10,"%.3f", sys.dis * 100.0f);
    if( sys.dis <= WAVE_THERSHOLD)
        return true;
    else return false;
}

//׼���׶�:Ƶ�ʳ�ʼ��,��Ϊ��·�ṩ����ʵķ�ֵ
void PrepareForTest(void)
{
    u16 PreCnt=0;		//׼������ֵ
    sinfrq.freq=1000;
    sinfrq.amp=500;
    ad9959_write_frequency(AD9959_CHANNEL_0, sinfrq.freq);
    ad9959_write_amplitude(AD9959_CHANNEL_0, sinfrq.amp);
    delay_ms(100);
    while(!isGoodWave())		//�жϷ�ֵ�Ƿ����
    {
    	sinfrq.amp -= 50;
    	ad9959_write_amplitude(AD9959_CHANNEL_0, sinfrq.amp);
        delay_ms(100);
        if(++PreCnt >= 8)
        {
            return;
        }
    }
}


//�������ֲ��ԣ����Ե�·�����ֲ���
void CalCircuitParam(Sys *param){

	/* */
	getADResults();			//ADS8688��·ͨ������

	calc_FFT(adResIn1,fftResult[0]);	//���������1FFT
	calc_FFT(adResIn2,fftResult[1]);	//���������2FFT
	inputVol[0]=fftResult[0][50];		//������Ե�1��λ
	inputVol[1] = fftResult[1][50];		//������Ե�2��λ

	calc_FFT(adResNoLoad,fftResult[3]);		//�������FFT
	outputVol[1] = fftResult[3][50];			//������ش���λ

	/* ���ز��� */
	relayLoad;
	delay_ms(200);

	getADResults();
	calc_FFT(adResLoad,fftResult[2]);
	outputVol[0]=fftResult[2][50];				//������ش���λ

	/* ���ݲ���ֵ�����·���� */
	param->inputRes = sys.Rs*inputVol[1]/(inputVol[0]-inputVol[1]);		//�����迹
	param->outputRes = sys.Ro*(outputVol[1]/outputVol[0]-1);			//����迹
	param->gain = outputVol[1]/inputVol[1];		//��·����

    /* �����쳣���� */
    if(param->inputRes < -1000)
    	ad9959_init();
    if(param->outputRes <= 0)
        param->outputRes = 1927.0f;

	/* �´ο��ز��� */
	relayNoLoad;		//�ٴο���
	delay_ms(200);

}

//�ҵ����޽�ֹƵ��
float dB2times[21]={1,0.891250938,0.794328235,0.707945784,0.630957344,0.562341325,0.501187234,0.446683592,0.398107171,0.354813389,0.316227766,0.281838293,0.251188643,0.223872114,0.199526231,0.177827941,0.158489319,0.141253754,0.125892541,0.112201845,0.1};
float calcUpFreq(float*array)
{
    u16 i;
    float value = dB2times[sys.dB],freq=0;
    for(i=sweepfreq.time-1;i>0;i--)
    {
        if(array[i-1]>value && array[i]<value)
        {
            freq = i*sweepfreq.step;
            break;
        }
    }
    if(i)
    freq = freq - sweepfreq.step*(value-array[i])/(array[i-1]-array[i]);
    return freq;
}

//ɨƵ����
void SweepTest(void)
{
    u16 i;float result;u32 useless;
    AD_arrInit();
    sweepfreq.start=0;Out_freq(0,sweepfreq.start);delay_ms(200);
    sweepfreq.step=10000;
    sweepfreq.end=200000;
    sweepfreq.time=(sweepfreq.end/sweepfreq.step)+1;
    for(i=0;i<sweepfreq.time;i++)  // 100Hz -> 200K,21��,����10 KHz
    {
    	ad9959_write_frequency(AD9959_CHANNEL_0, sweepfreq.start + i*sweepfreq.step);
        delay_ms(100);
        get_ADS_allch(pAD_array++);
    }
    sinfrq.freq = 1000;
    ad9959_write_frequency(AD9959_CHANNEL_0,sinfrq.freq);
    /* ��һ������ */
    arm_max_f32(adResRMS,i,&result,&useless);
    if(result==0)result=1;    //��ֹ��������
    arm_scale_f32(adResRMS, 1.0f/result, adResRMS,i);
    SetTFTText(0,36,"%.1fkHz",calcUpFreq(adResRMS)*0.001f);
    pTFT_array = TFT_array;pAD_array = adResRMS;
    arm_scale_f32(adResRMS, 255.0f, adResRMS,i);
    for(i=0;i<sweepfreq.time;i++)
        *pTFT_array++ = *pAD_array++;
    GraphChannelDataAdd(0,38,0,TFT_array,i);
}

//��չɨƵ����
void ExSweepTest(void)
{
    u16 i;float result;u32 useless;
    AD_arrInit();
    sweepfreq.start=0;Out_freq(0,sweepfreq.start);delay_ms(200);
    sweepfreq.step=2000;
    sweepfreq.end=200000;
    sweepfreq.time=(sweepfreq.end/sweepfreq.step)+1;
    for(i=0;i<sweepfreq.time;i++)  // 100Hz -> 200K,101��,����1 KHz
    {
    	ad9959_write_frequency(AD9959_CHANNEL_0, sweepfreq.start + i*sweepfreq.step);
        delay_ms(20);
        get_ADS_allch(pAD_array++);
    }
    sinfrq.freq = 1000;
    ad9959_write_frequency(AD9959_CHANNEL_0,sinfrq.freq);
    /* ��һ������ */
    arm_max_f32(adResRMS,i,&result,&useless);
    if(result==0)result=1;    //��ֹ��������
    arm_scale_f32(adResRMS, 1.0f/result, adResRMS,i);
    pTFT_array = TFT_array;pAD_array = adResRMS;
    arm_scale_f32(adResRMS, 255.0f, adResRMS,i);
    for(i=0;i<sweepfreq.time;i++)
        *pTFT_array++ = *pAD_array++;
    GraphChannelDataAdd(1,3,0,TFT_array,i);
}


//�ж�һ���Ƿ��ڴ�����
bool isRound(float newNum,float min,float max)
{
    if(  (newNum>min)
       &&(newNum<max)
      )
    return true;
    else return false;
}

//���Ӳ��ֵ�·������ʾ
void CircuitFaultShow(u16 error)
{
    if(error)
        SetTFTText(0,22,(char*)"����");
    else
        SetTFTText(0,22,(char*)"����");
    SetTFTText(0,23,(char*)(u8*)CiucuitErrors[error]);
//    if(error == 10) delay_ms(1000);
    delay_ms(20);
}

//���ϲ�����չUI�������
void ExCircuitFaultShow(u16 error)
{
//    SetControlVisiable(2,(error+7)/2,1);

    SetTFTText(2,3,(char*)(u8*)CiucuitErrors[error]);

//    SetControlVisiable(2,(sys.err+7)/2,0);
    delay_ms(20);

//    sys.err = error;

}


//���ϲ��ֲ���
u16 CalCircuitError(void){

	CalResIn_AC(&sysError);	//�����������

	if(isRound(sysError.inputRes,13500,15000)){
		delay_ms(300);
		return R1ErrorOpen;				//R1��·
	}

	else if(sysError.inputRes>20000){
		delay_ms(300);
		return C1ErrorOpen;		//C1��·
	}


	getDC(&sysError);		//��ȡֱ����ѹ
	Out_mV(0,500);
	delay_ms(200);

	if(isRound(sysError.inputRes,9000,12000)){
		if(isRound(sysError.DC,3800,4000)){
			return R4ErrorOpen;			//R4��·
		}
		else if(isRound(sysError.AC,10,20)){
			return C2ErrorOpen;			//C2��·
		}
	}

	else if(sysError.inputRes<200){
		if(isRound(sysError.DC,3500,3800)){
			return R1ErrorShort;		//R1��·
		}
		else if(isRound(sysError.DC,1200,1600)){
			return R2ErrorOpen;			//R2��·
		}
		else if(isRound(sysError.DC,3800,4000)){
			return R2ErrorShort;		//R2��·
		}
		else if(isRound(sysError.DC,190,300)){
			return R3ErrorOpen;			//R3��·
		}
		else if(isRound(sysError.DC,100,180)){
			return R4ErrorShort;		//R4��·
		}
	}


	else if(isRound(sysError.inputRes,2000,4000)){
		if(isRound(sysError.DC,3800,4000)){
			return R3ErrorShort;		//R3��·
		}

		CheckAmp(2);
		if(ampValue[1] / sys.amp2 > 1.6f){
			return C2ErrorTwice;		//C2�ӱ�
		}
		else if(ampValue[2] / sys.amp3 > 1.12f){
			return C3ErrorOpen;			//C3��·
		}
		else if(ampValue[2] / sys.amp3 < 0.85f){
			return C3ErrorTwice;		//C3�ӱ�
		}

	}

//	CheckAmp(0);
//	if(judgeC1 > 2.0f){
//		return C1ErrorTwice;		//C1�ӱ�
//	}

	return NoError;
}

//��ȡֱ����ѹ
void getDC(Sys* param)
{
    Out_mV(0,0);delay_ms(200);
    pAD_array=AD_array[0];
    get_ADS_allch(pAD_array);
    param->DC = adResDC[0];
}

//�����·�������
void CalResIn_AC(Sys *param){

	getADResults();

	calc_FFT(adResIn1,fftResult[0]);	//���������1FFT
	calc_FFT(adResIn2,fftResult[1]);	//���������2FFT
	calc_FFT(adResNoLoad,fftResult[3]);	//�������FFT
	inputVol[0]=fftResult[0][50];		//������Ե�1��λ
	inputVol[1]=fftResult[1][50];		//������Ե�2��λ

	//�����·����
	param->inputRes = sys.Rs*inputVol[1]/(inputVol[0]-inputVol[1]);	//�����迹
	param->AC = fftResult[3][50];		//������ֵ

    if(param->inputRes < -1000)
    {
    	ad9959_init();
    }
}


//����Ƶ����,����1��һ������
u16 CheckAmp(u16 part)
{
    /* ����������Ƶ��ֵ */
    pAD_array=AD_array[0];

    get_ADS_allch(pAD_array);
    ampValue[0] = adResRMS[0];
    if(ampValue[0]<0)ampValue[0]=1;

    if(sys.mode==BaseTest)sys.amp1=ampValue[0];

    if(part==0)
    {
    	relay2Load;
    	ad9959_write_frequency(AD9959_CHANNEL_0, 20);
    	ad9959_write_amplitude(AD9959_CHANNEL_0, 500);
        delay_ms(200);
        getADResults();
        cal_fftPhase(adResIn1,fftPhase[0]);
        cal_fftPhase(adResNoLoad,fftPhase[3]);
        phaseTwo[0] = fftPhase[0][1];
        phaseTwo[1] = fftPhase[3][1];
        if(sys.mode==BaseTest){
        	sys.skewing=phaseTwo[0]-phaseTwo[1];
        	if(sys.skewing < 0) sys.skewing += 360;
        	sys.RmsForC1 = adResRMS[0];
        }
        skewing =phaseTwo[0]-phaseTwo[1];
        if(skewing < 0) skewing += 360;
        RmsForC1 = adResRMS[0];

        judgeC1 = skewing - sys.skewing;
        judgeC1Rms = RmsForC1 - sys.RmsForC1;

        relay2NoLoad;
    }
    else if(part==1)
    {
        Out_freq(0,outFreq[2]);		//���100kHz
        delay_ms(300);
        get_ADS_allch(pAD_array);
        ampValue[2] = adResRMS[0];
    }
    else
    {
    	/* ���100kHz */
    	relay2Load;
    	ad9959_write_frequency(AD9959_CHANNEL_0, 100000);
    	ad9959_write_amplitude(AD9959_CHANNEL_0, 500);
        pAD_array=AD_array[0];
        delay_ms(300);
        get_ADS_allch(pAD_array);
        ampValue[2] = adResRMS[0];
        if(sys.mode==BaseTest)sys.amp3=ampValue[2];
        relay2NoLoad;

        /* ���50Hz */
        ad9959_write_frequency(AD9959_CHANNEL_0, 50);
        ad9959_write_amplitude(AD9959_CHANNEL_0, 500);
        pAD_array=AD_array[0];
        delay_ms(800);
        get_ADS_allch(pAD_array);
        ampValue[1] = adResRMS[0];
        if(sys.mode==BaseTest)sys.amp2=ampValue[1];

    }

    sinfrq.freq = 1000;
    ad9959_write_frequency(AD9959_CHANNEL_0, 1000);

    if(isRound(ampValue[1],goodAmp[1]*0.96f,goodAmp[1]*1.04f)
     &&isRound(ampValue[2],goodAmp[2]*0.96f,goodAmp[2]*1.04f))
        return 1;
    else return 0;
}

