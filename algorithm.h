/*
 * algorithm.h
 *
 *  Created on: Jun 24, 2022
 *      Author: ��־��
 */

#ifndef INC_ALGORITHM_H_
#define INC_ALGORITHM_H_

#include "base.h"
#include "stdbool.h"
#include "arm_math.h"

#define AD_Size 256

#define BaseTest   1		//�������ֲ���
#define UpTest     2		//���Ӳ��ֲ���
#define ExAmpFreq  3		//��չ��Ƶ�������߲���
#define ExElecTest 4		//��չ���ϲ��ֲ���

/* ��·����ԭ�� */
#define NoError         0
#define R1ErrorOpen     1
#define R1ErrorShort    2
#define R2ErrorOpen     3
#define R2ErrorShort    4
#define R3ErrorOpen     5
#define R3ErrorShort    6
#define R4ErrorOpen     7
#define R4ErrorShort    8
#define C1ErrorOpen     9
#define C1ErrorTwice    10
#define C2ErrorOpen     11
#define C2ErrorTwice    12
#define C3ErrorOpen     13
#define C3ErrorTwice    14



typedef struct _Sys
{
    short mode;         //����ģʽ

    u16 AmpFreqFlag;    //�Ƿ��Ƶ���Բ���
    float inputRes;     //�������
    float outputRes;    //�������
    float gain;         //��·����
    float beta;         //����ֵ

    float upFreq;       //����Ƶ��
    u16 dB;           //��Ӧ�ֱ���

    short bugElec;      //����Ԫ��
    u8*   bugResult;    //����ԭ��

    float Rs;           //��������ѹ����
    float Ro;           //��������ѹ����
    float DC;           //ֱ����ƽ
    float AC;			//�������ֵ
    float dis;          //ʧ���
    u16   Auto;         //�л��Զ������ֶ���
    float InputRes15;
    float R1R2Short;
    float R3R4Open;
    float skewing;		//��λ��
    float RmsForC1;
    float amp1;			//1kHz
    float amp2;			//10Hz
    float amp3;			//100kHz
    u16 flag;
    u16 err;
}Sys;
extern Sys sys;

typedef struct _ArrayParam   //��������ṹ��
{
    float max;      //���ֵ
    float min;      //��Сֵ
    float tft_len;  //����һ������ռ��������
    float tft_cycle;
    float Vpp;   //���ֵ mV
    float Period;//����   us
    float Aver;  //ƽ��ֵ mv
    float Rms;   //��Чֵ mv
}ArrayParam;
extern ArrayParam AD_Params;

void AD_arrInit(void);
void getADResults(void);
void Params_Init(void);
void calc_FFT(float*Input,float*Output);
void PowerPhaseRadians_f32(float32_t *_ptr, float32_t *_phase, uint16_t _usFFTPoints, float32_t _uiCmpValue);

bool isGoodWave(void);
void PrepareForTest(void);

void CalCircuitParam(Sys *param);

void SweepTest(void);
void ExSweepTest(void);

bool isRound(float newNum,float min,float max);
void CircuitFaultShow(u16 error);
void ExCircuitFaultShow(u16 error);
u16 CalCircuitError(void);
void getDC(Sys* param);
void CalResIn_AC(Sys *param);

u16 CheckAmp(u16 part);

#endif /* INC_ALGORITHM_H_ */
