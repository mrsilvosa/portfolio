/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "usbd_customhid.h"

#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "MY_ILI9341.h"
#include "SD_SPI.h"
#include "ILI9341_BSP_F4xx.h"
#include "SD_SPI_BSP_F4xx.h"
#include "SD_SPI_+_ILI9341.h"
#include "Wire.h"
#include "AS726X.h"
#include "Adafruit_TCS34725.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum{
	MOVE_UP=0,
	MOVE_DOWN=1,
	MOVE_LEFT=2,
	MOVE_RIGHT=3,
	SELECTION=4,
	IDLE=5
}inputs_t;

typedef struct
{
	union
	{
		uint8_t actions;
		struct
		{
			inputs_t inputs:3;
			inputs_t input_activated:3;
			uint8_t pulsed:1;
			uint8_t action_pending:1;
		};
	};
}userInteraction_t;

typedef enum{
	UP_LEFT=0,
	UP_MIDDLE,
	UP_RIGHT,
	DOWN_LEFT,
	DOWN_MIDDLE,
	DOWN_RIGHT
}menuPosition_t;

typedef struct
{
	union
	{
		uint16_t shorty;
		struct
		{
			uint8_t byte_LSB;
			uint8_t byte_MSB;
		};
	};
}shorty_bytes_t;

typedef struct
{
	uint8_t rotation;
	uint8_t petition;
	image_select_t imageSelected;
	menuPosition_t positionReceived;
	bool MENU_isSelected;
	uint8_t dataVar;

}T_ILI9341_action_t;

typedef struct
{
	uint16_t block;
	uint8_t petition;
	image_select_t imageSelected;

}T_SD_action_t;

typedef enum{
	BUFFER_IDLE=0,
	BUFFER_READY,
	BUFFER_LOADING,
	BUFFER_PRINTING
}bufferState_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c3;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi1_tx;
DMA_HandleTypeDef hdma_spi2_tx;
DMA_HandleTypeDef hdma_spi2_rx;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */

/**************** Sensor espectro: ******************/
AS726X_t AS7262;
I2C_HandleTypeDef* AS726X_I2C_HANDLE;	// inicializar con el i2c correspond.
const uint8_t GAIN = 2;
const uint8_t MEASUREMENT_MODE = 3;

// ES 296 pero 320 por las dudas ya que usb recibe de a 64 bytes.
float intensity_I_measured[320]; // guarda los datos pertinentes de intensidad.
bool intensity_saved = false;

/****************  Pantalla TFT: ******************/
ILI9341_t ILI9341;

// pantalla Hardware requisitos:
ILI9341_BSP_variables_t ILI9341_HW={
		.lcdSPIhandle = &hspi1,
		.tftCS_GPIO = TFT_SPI_CS_GPIO_Port,
		.tftCS_PIN = TFT_SPI_CS_Pin,
		.tftDC_GPIO = TFT_DC_GPIO_Port,
		.tftDC_PIN = TFT_DC_Pin,
		.tftRESET_GPIO = TFT_RESET_GPIO_Port,
		.tftRESET_PIN = TFT_RESET_Pin
};

/****************  Para el sistema de alternancia de buffers: ******************/
uint8_t bufferRx[514];
uint8_t bufferTx[514]={[0 ... 513]=0xFF};
uint8_t dummyBufferTx[514]={[0 ... 513]=0xFF};
uint8_t dummyBufferRx[514]={[0 ... 513]=0xAA};
uint8_t buffers[4][514];
volatile bufferState_t buffer_ready[4]={[0 ... 3]= BUFFER_IDLE};

/**** Para recuperar el espectro desde SD y luego manejar la intensidad: *******/

// El espectro es de aproximadamente 296x101 = 29896px = 59792 bytes
uint16_t bufferEspectro[296*101];  // guarda el espectro para el algoritmo.
// -> uint16_t pixel = bufferEspectro[ y * 296 + x]
// posicionamiento, rectángulo donde se encuentra el espectro en pantalla:
const uint16_t Espectro_y1 = 30, Espectro_x1 = 13, Espectro_y2 = 130, Espectro_x2 = 308;

/****************  tarjeta SD SPI: ******************/
// SD: indicar en main lo que vale c/u.
SPI_HandleTypeDef* SD_SPI_HANDLE;
GPIO_TypeDef* SD_CS_PORT;
uint16_t SD_CS_PIN;

/**************************  USB: **************************/
// USB variables:
uint8_t tx_buffer[64];		//Variable to store the output data
uint8_t report_buffer[64];		//Variable to receive the report buffer
uint8_t flag = 0;			//Variable to store the button flag
uint8_t flag_rx = 0;			//Variable to store the reception flag
//extern the USB handler:
extern USBD_HandleTypeDef hUsbDeviceFS;

/**************************  Semáforos FRTOS: **************************/

SemaphoreHandle_t S_MdE_pulsador; // lo libera MdE, lo toma la tarea pulsadores
SemaphoreHandle_t S_pulsador;	// lo libera la tarea pulsadores
SemaphoreHandle_t S_ILI9341;	// lo libera tarea ILI9341
SemaphoreHandle_t S_ILI9341_ISR;	// lo libera SPI, lo toma tarea ILI9341
SemaphoreHandle_t S_SD;	// lo libera tarea SD
SemaphoreHandle_t S_SD_ISR;	// lo libera SPI, lo toma tarea SD

/**************************  Colas FRTOS: **************************/
// Son paquetes que contienen directivas de accionar:
QueueHandle_t Q_pulsadoresEstado;
QueueHandle_t Q_ILI9341;
QueueHandle_t Q_SD;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C3_Init(void);
static void MX_TIM3_Init(void);
void StartDefaultTask(void const * argument);

/* USER CODE BEGIN PFP */

menuPosition_t MENU_posicionamiento(userInteraction_t inputUserReceived);
void MENU_desplazamiento_print(menuPosition_t positionReceived, bool isSelected);
void MENU_desplazamiento_erase(menuPosition_t positionReceived);
menuPosition_t ESPECTRO_posicionamiento(userInteraction_t inputUserReceived);
void ESPECTRO_desplazamiento_print(menuPosition_t positionReceived, bool isSelected);
void ESPECTRO_desplazamiento_erase(menuPosition_t positionReceived);
void ESPECTRO_algoritmoIntensidadEspectro(void);
float gauss(float x, float mean, float sigma);
float intensity_from_wavelength(float lambda, float Rn, float Gn, float Bn);
void ESPECTRO_cargarEspectroColor(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, image_select_t imageSelected);
menuPosition_t OPCIONES_posicionamiento(userInteraction_t inputUserReceived);
void OPCIONES_desplazamiento_print(menuPosition_t positionReceived, bool isSelected, char* integrationTimeText, char* gainText);
void OPCIONES_desplazamiento_erase(menuPosition_t positionReceived, char* integrationTimeText, char* gainText);

int8_t USB_task(void); // su tiempo de vida es corto, no vale la pena hacerlo una tarea.
void speed_up_SPI(void);

void T_LDR(void* args);
void T_SD(void* args);
void T_ILI9341(void* args);
void T_MdE(void* arg);
void T_detectar_pulsador(void* arg);

/**************** Tareas del sistema ********************/

void T_LDR(void* args)
{
	const float DUTY_MIN = 0.05f; // duty minimo arbitrario
	const float DUTY_MAX = 1.0f;
	const uint16_t PWM_PERIODO = 1000; // ARR del tim4
	const uint32_t delay = 100;	// 100ms de espera entre lecturas

	uint32_t ADC_valor = 0;
	float valor_normalizado = 0.0f;
	float duty[10] = {[0 ... 9]=0.0f};
	float duty_promedio = 0.0f;
	uint32_t PWM_valor = 0;
	uint32_t duty_index = 0;
	const uint32_t duty_amount_max = 10;

	while(1)
	{
		HAL_ADC_Start(&hadc1); // empiezo lectura adc

		// defino un timeout arbitrario y espero a que el adc esté ready:
		if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
		{
			ADC_valor = HAL_ADC_GetValue(&hadc1); // 12 bits, 0 a 4095
		}
		HAL_ADC_Stop(&hadc1);

		// tomo el porcentaje según mi valor máximo del duty:
		valor_normalizado = 1.0f - ((float)ADC_valor / 4095.0f);

		// igualo al duty teniendo en cuenta el valor mínimo:
		duty[duty_index] = DUTY_MIN + valor_normalizado * (DUTY_MAX - DUTY_MIN);


		// limito a maximos y minimos:
		if(duty[duty_index] < DUTY_MIN){ duty[duty_index] = DUTY_MIN; }
		if(duty[duty_index] > DUTY_MAX){ duty[duty_index] = DUTY_MAX; }

		duty_promedio = 0.0f;

		for(uint32_t i = 0; i < duty_amount_max; i++)
		{
			duty_promedio += duty[i];
		}
		duty_promedio /= duty_amount_max;

		if(duty_promedio < DUTY_MIN){ duty_promedio = DUTY_MIN; }
		if(duty_promedio > DUTY_MAX){ duty_promedio = DUTY_MAX; }

		PWM_valor = (uint32_t)(duty_promedio * PWM_PERIODO); // insertamos para el ARR

		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, PWM_valor);

		duty_index++;
		duty_index %= duty_amount_max;
		vTaskDelay(delay); // Espero a la siguiente toma de valores para no saturar.

	}
}

void T_SD(void* args)
{
	static T_SD_action_t userSDPetition;
	static uint32_t bufferSDIndex = 0;
	static uint32_t blockSDIndex = 0;
	static bool SD_imageLoading = false;
	static bool success = false;
	while(1)
	{
		// Espero que me llamen para buscar algo, lo que sea:
		xQueueReceive(Q_SD, &userSDPetition, portMAX_DELAY);

		// analizo qué me pidieron:
		switch(userSDPetition.petition)
		{
		case 1:
			// load image:
			// iteramos 300 bloques:
			// trabajará en equipo con la tarea del ILI9341:
			// se utilizan buffers globales y flags globales por la velocidad.
			SD_imageLoading = true;
			while(SD_imageLoading)
			{
				if((buffer_ready[bufferSDIndex] == BUFFER_LOADING))
				{
					// Espero que termine el SPI:
					xSemaphoreTake(S_SD_ISR, portMAX_DELAY);

					//volatile HAL_SPI_StateTypeDef st = hspi2.State;
					//volatile uint32_t err = HAL_SPI_GetError(&hspi2);

					SD_BSP_SPI_transmitReceive_DMA_waitReady();
					SD_BSP_SPI_transmitReceive_DMA_finish();

					buffer_ready[bufferSDIndex] = BUFFER_READY;
					bufferSDIndex++;
					bufferSDIndex %= 4;

					sd_wait_ready();
					if(blockSDIndex >= 300)
					{
						SD_imageLoading = false;
					}
				}
				if(SD_imageLoading == false)
				{
					continue;
				}

				if((buffer_ready[bufferSDIndex] == BUFFER_IDLE))
				{
					success = true;	// state inicial para saltear el error state.
					do
					{
						if(!success)
						{
							sd_wait_ready();
						}
						success = sd_read_block_DMA(userSDPetition.imageSelected + blockSDIndex, dummyBufferTx, buffers[bufferSDIndex]);
					}while(!success);

					buffer_ready[bufferSDIndex] = BUFFER_LOADING;
					blockSDIndex++;
				}
			}
			bufferSDIndex = 0;
			blockSDIndex = 0;
			break;
		case 2:
			// rescatar solamente la parte espectro de ESPECTRO_COLOR:
			image_select_t image = ESPECTRO_COLOR;
			ESPECTRO_cargarEspectroColor(Espectro_x1, Espectro_y1, Espectro_x2, Espectro_y2, image);
			break;
		default:
			// no me pidieron nada
			break;
		}

		xSemaphoreGive(S_SD);
	}
}

void T_ILI9341(void* args)
{
	static T_ILI9341_action_t userDisplayPetition;
	static uint32_t bufferILI9341Index = 0;
	static uint32_t blockILI9341Index = 0;
	static bool ILI9341_imagePrinting = false;
	static bool isFirstBlock = false;
	static uint32_t width = 0;
	static uint32_t height = 0;
	static char stringTimeout[] = "TIMEOUT: 5 SEG ..";
	stringTimeout[9] = '5';

	static char integrationTimeText[10] = "2.5 ms";
	static char gainText[10] = "x4";

	static uint8_t integrationTimeTextIndex = 0;
	const uint8_t max_integrationTimeTextIndex = 19;
	static uint8_t integrationTimeActive = TCS34725_INTEGRATIONTIME_2_4MS;
	static uint8_t integrationTimeSelected = TCS34725_INTEGRATIONTIME_2_4MS;

	static uint8_t gainTextIndex = 0;
	const uint8_t max_gainTextIndex = 4;
	static uint8_t gainActive = TCS34725_GAIN_4X;
	static uint8_t gainSelected = TCS34725_GAIN_4X;

	while(1)
	{
		// Espero que me llamen para poner algo en pantalla, lo que sea:
		xQueueReceive(Q_ILI9341, &userDisplayPetition, portMAX_DELAY);

		if(userDisplayPetition.rotation == 3 || userDisplayPetition.rotation == 1)
		{
			width = 240;
			height = 320;
		}
		else
		{
			width = 320;
			height = 240;
		}

		// analizo qué me pidieron:
		switch(userDisplayPetition.petition)
		{
		case 0:
			// print BLACK screen:
			ILI9341.setRotation(2);
			ILI9341.fill_DMA(COLOR_BLACK, width, height, bufferRx);
			break;
		case 1:
			// print whole image:
			// se iterarán 300 bloques:
			// trabajará en equipo con la tarea del SD:
			// se utilizan buffers globales y flags globales por la velocidad.
			ILI9341.setRotation(userDisplayPetition.rotation);
			if(userDisplayPetition.rotation == 3 || userDisplayPetition.rotation == 1)
			{
				width = 240;
				height = 320;
			}
			else
			{
				width = 320;
				height = 240;
			}
			ILI9341_imagePrinting = true;
			isFirstBlock = true;
			while(ILI9341_imagePrinting)
			{
				if(buffer_ready[bufferILI9341Index] == BUFFER_PRINTING)
				{
					// Espero que termine el SPI:
					xSemaphoreTake(S_ILI9341_ISR, portMAX_DELAY);
					ILI9341.printImageSector_512bytes_DMA_finish();

					buffer_ready[bufferILI9341Index] = BUFFER_IDLE;
					bufferILI9341Index++;
					bufferILI9341Index %= 4;
					if(blockILI9341Index >= 300)
					{
						ILI9341_imagePrinting = false;
					}
				}
				if(ILI9341_imagePrinting == false)
				{
					continue;
				}
				if(buffer_ready[bufferILI9341Index] == BUFFER_READY)
				{

					ILI9341.printImageSector_512bytes_DMA_start(0, 0, width, height, buffers[bufferILI9341Index], isFirstBlock);
					buffer_ready[bufferILI9341Index] = BUFFER_PRINTING;
					isFirstBlock = false;
					blockILI9341Index++;
				}
			}
			bufferILI9341Index = 0;
			blockILI9341Index = 0;
			break;
		case 2:
			// Imprimo el default selected del Menú:

			// X es 320 e Y es 240, X inicial está en J4, Y final está en VCC
			//ILI9341_Fill_Rect(10+x, 200+x, 310+x, 210+x, COLOR_MAGENTA);
			ILI9341.fillRectangle(10, 10, 109, 15, COLOR_BLUE); // lado superior
			ILI9341.fillRectangle(10, 10, 15, 96, COLOR_BLUE); // lado izquierdo
			ILI9341.fillRectangle(10, 91, 109, 96, COLOR_BLUE); // lado inferior
			ILI9341.fillRectangle(104, 10, 109, 96, COLOR_BLUE); // lado derecho
			break;
		case 3:
			// MENU_desplazamiento pide que borre:
			MENU_desplazamiento_erase(userDisplayPetition.positionReceived);
			break;
		case 4:
			// MENU_desplazamiento pide que imprima:
			MENU_desplazamiento_print(userDisplayPetition.positionReceived, userDisplayPetition.MENU_isSelected);
			break;
		case 5:
			// ESPECTRO_desplazamiento pide que borre:
			ESPECTRO_desplazamiento_erase(userDisplayPetition.positionReceived);
			break;
		case 6:
			// ESPECTRO_desplazamiento pide que imprima:
			ESPECTRO_desplazamiento_print(userDisplayPetition.positionReceived, userDisplayPetition.MENU_isSelected);
			break;
		case 7:
			// ESPECTRO_cargarEspectro en pantalla:
			ESPECTRO_algoritmoIntensidadEspectro();
			break;
		case 8:
			// espectro cargar pop up:
			ILI9341.fillRectangle(20, 50, 280, 150, COLOR_BLUE);
			ILI9341.printText("ESPECTRO GUARDADO", 25,  80, COLOR_WHITE, COLOR_BLUE, 2);
			ILI9341.printText("VOLVIENDO AL MENU...", 25,  110, COLOR_WHITE, COLOR_BLUE, 2);
			break;
		case 9:
			// opciones mostrar:
			ILI9341.setRotation(userDisplayPetition.rotation);
			ILI9341.fill_DMA(COLOR_YELLOW, width, height, bufferRx);
			ILI9341.printText("SELECCIONE PARA ALTERNAR:", 5, 5, COLOR_BLACK, COLOR_YELLOW, 2);
			ILI9341.printText("TIEMPO INTEGRACION:", 5,  45, COLOR_BLACK, COLOR_YELLOW, 2);
			ILI9341.printText(integrationTimeText, 230,  45, COLOR_BLACK, COLOR_YELLOW, 2);
			ILI9341.printText("GANANCIA:", 5, 75, COLOR_BLACK, COLOR_YELLOW, 2);
			ILI9341.printText(gainText, 230, 75, COLOR_BLACK, COLOR_YELLOW, 2);
			ILI9341.printText("APLICAR CAMBIOS", 5, 135, COLOR_BLACK, COLOR_YELLOW, 2);
			ILI9341.printText("VOLVER AL MENU", 5, 165, COLOR_BLACK, COLOR_YELLOW, 2);
			break;
		case 10:
			// usb cargar pop up:
			ILI9341.fillRectangle(20, 50, 280, 150, COLOR_BLUE);
			ILI9341.printText("NO HAY DATOS", 25,  80, COLOR_WHITE, COLOR_BLUE, 2);
			ILI9341.printText("VOLVIENDO AL MENU...", 25,  110, COLOR_WHITE, COLOR_BLUE, 2);
			break;
		case 11:
			// cargo pantalla de espera de recepcion USB con timeout:
			stringTimeout[9] = '5';
			ILI9341.setRotation(userDisplayPetition.rotation);
			ILI9341.fill_DMA(COLOR_CYAN, width, height, bufferRx);
			ILI9341.printText("ESPERANDO PC...", 25,  80, COLOR_BLACK, COLOR_CYAN, 2);
			ILI9341.printText(stringTimeout, 25,  110, COLOR_BLACK, COLOR_CYAN, 2);
			break;
		case 12:
			// decremento timeout y actualizo:
			stringTimeout[9] --;
			ILI9341.printText(stringTimeout, 25,  110, COLOR_BLACK, COLOR_CYAN, 2);
			break;
		case 13:
			// conectado con pc, por favor espere:
			ILI9341.setRotation(userDisplayPetition.rotation);
			ILI9341.fill_DMA(COLOR_GREEN, width, height, bufferRx);
			ILI9341.printText("CONECTADO CON PC", 25,  80, COLOR_BLACK, COLOR_GREEN, 2);
			ILI9341.printText("ENVIANDO DATOS", 25,  110, COLOR_BLACK, COLOR_GREEN, 2);
			ILI9341.printText("POR FAVOR ESPERE...", 25,  140, COLOR_BLACK, COLOR_GREEN, 2);
			break;
		case 14:
			// datos enviados con exito, volviendo:
			ILI9341.setRotation(userDisplayPetition.rotation);
			ILI9341.fill_DMA(COLOR_MAGENTA, width, height, bufferRx);
			ILI9341.printText("ENVIO DE DATOS", 25,  80, COLOR_BLACK, COLOR_MAGENTA, 2);
			ILI9341.printText("REALIZADO CON EXITO", 25,  110, COLOR_BLACK, COLOR_MAGENTA, 2);
			ILI9341.printText("VOLVIENDO AL MENU ...", 25,  140, COLOR_BLACK, COLOR_MAGENTA, 2);
			break;
		case 15:
			// error al mandar datos, volviendo:
			ILI9341.setRotation(userDisplayPetition.rotation);
			ILI9341.fill_DMA(COLOR_RED, width, height, bufferRx);
			ILI9341.printText("ENVIO DE DATOS", 25,  80, COLOR_BLACK, COLOR_RED, 2);
			ILI9341.printText("FALLIDO", 25,  110, COLOR_BLACK, COLOR_RED, 2);
			ILI9341.printText("VOLVIENDO AL MENU ...", 25,  140, COLOR_BLACK, COLOR_RED, 2);
			break;
		case 16:
			// OPCIONES_desplazamiento pide que borre:
			OPCIONES_desplazamiento_erase(userDisplayPetition.positionReceived, integrationTimeText, gainText);
			break;
		case 17:
			// OPCIONES_desplazamiento pide que imprima:
			switch(userDisplayPetition.dataVar)
			{
			case 1:
				// cambio al próximo texto de integration time:
				switch(integrationTimeTextIndex)
				{
				case 0:
					strcpy(integrationTimeText, "24 ms ");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_24MS;
					break;
				case 1:
					strcpy(integrationTimeText, "50 ms ");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_50MS;
					break;
				case 2:
					strcpy(integrationTimeText, "60 ms ");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_60MS;
					break;
				case 3:
					strcpy(integrationTimeText, "101 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_101MS;
					break;
				case 4:
					strcpy(integrationTimeText, "120 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_120MS;
					break;
				case 5:
					strcpy(integrationTimeText, "154 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_154MS;
					break;
				case 6:
					strcpy(integrationTimeText, "180 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_180MS;
					break;
				case 7:
					strcpy(integrationTimeText, "199 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_199MS;
					break;
				case 8:
					strcpy(integrationTimeText, "240 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_240MS;
					break;
				case 9:
					strcpy(integrationTimeText, "300 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_300MS;
					break;
				case 10:
					strcpy(integrationTimeText, "360 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_360MS;
					break;
				case 11:
					strcpy(integrationTimeText, "401 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_401MS;
					break;
				case 12:
					strcpy(integrationTimeText, "420 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_420MS;
					break;
				case 13:
					strcpy(integrationTimeText, "480 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_480MS;
					break;
				case 14:
					strcpy(integrationTimeText, "499 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_499MS;
					break;
				case 15:
					strcpy(integrationTimeText, "540 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_540MS;
					break;
				case 16:
					strcpy(integrationTimeText, "600 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_600MS;
					break;
				case 17:
					strcpy(integrationTimeText, "614 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_614MS;
					break;
				case 18:
					strcpy(integrationTimeText, "2.4 ms");
					integrationTimeSelected = TCS34725_INTEGRATIONTIME_2_4MS;
					break;
				default:
					break;
				}
				integrationTimeTextIndex++;
				integrationTimeTextIndex %= max_integrationTimeTextIndex;
				break;
			case 2:
				// cambio al próximo texto de gain:
				switch(gainTextIndex)
				{
				case 0:
					strcpy(gainText, "x16");
					gainSelected = TCS34725_GAIN_16X;
					break;
				case 1:
					strcpy(gainText, "x60");
					gainSelected = TCS34725_GAIN_60X;
					break;
				case 2:
					strcpy(gainText, "x1 ");
					gainSelected = TCS34725_GAIN_1X;
					break;
				case 3:
					strcpy(gainText, "x4 ");
					gainSelected = TCS34725_GAIN_4X;
					break;
				default:
					break;
				}
				gainTextIndex++;
				gainTextIndex %= max_gainTextIndex;
				break;
			case 3:
				// apply changes:
				gainActive = gainSelected;
				integrationTimeActive = integrationTimeSelected;
				Adafruit_TCS34725_setGain(gainActive);
				Adafruit_TCS34725_setIntegrationTime(integrationTimeActive);
				break;
			default:
				break;
			}
			OPCIONES_desplazamiento_print(userDisplayPetition.positionReceived, userDisplayPetition.MENU_isSelected, integrationTimeText, gainText);
			break;
		default:
			break;
		}

		xSemaphoreGive(S_ILI9341);
	}
}

void T_MdE(void* arg)
{
	static uint32_t timeout = 0;
	static uint8_t MdE = 0;
	static userInteraction_t inputUserReceived;
	static menuPosition_t MENU_previousStage = UP_LEFT;
	static menuPosition_t MENU_postStage = UP_LEFT;
	static menuPosition_t ESPECTRO_previousStage = DOWN_LEFT;
	static menuPosition_t ESPECTRO_postStage = DOWN_LEFT;
	static menuPosition_t OPCIONES_previousStage = UP_LEFT;
	static menuPosition_t OPCIONES_postStage = UP_LEFT;
	static T_ILI9341_action_t ILI9341_action={
			.imageSelected = LOGO,
			.petition = 0,
			.rotation = 0
	};
	static T_SD_action_t SD_action={
			.block = 0,
			.imageSelected = LOGO,
			.petition = 0
	};

	while(1)
	{
		switch(MdE)
		{
		case 0:
			ILI9341_action.petition = 0;	// set screen BLACK
			ILI9341_action.rotation = 2;
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			MdE = 1;
			break;
		case 1:
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);

			ILI9341_action.petition = 1;	// load whole image
			ILI9341_action.rotation = 3;
			ILI9341_action.imageSelected = LOGO;
			SD_action.imageSelected = LOGO;
			SD_action.petition = 1;
			xQueueSendToBack(Q_SD, &SD_action, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			vTaskDelay(3000);
			MdE = 2;
			break;
		case 2:
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			// Espero que esté sin usar la SD:
			xSemaphoreTake(S_SD, portMAX_DELAY);

			ILI9341_action.petition = 1;
			ILI9341_action.rotation = 2;
			ILI9341_action.imageSelected = MENU; // imprimo menu principal:
			SD_action.imageSelected = MENU;
			SD_action.petition = 1;
			xQueueSendToBack(Q_SD, &SD_action, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);

			// Espero que esté libre la pantalla:
			//xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			//ILI9341_action.petition = 2;	// imprimo default selected
			//ILI9341_action.rotation = 2;
			//xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);

			userInteraction_t aux2 = inputUserReceived;
			aux2.input_activated = IDLE;
			MENU_posicionamiento(aux2); // otra forma de poner la selección

			MdE = 3;
			xSemaphoreGive(S_MdE_pulsador);
			break;
		case 3:
			// espero interactuar con el usuario en menú principal:
			//sempaforeTake(pulsadores); o queueTake(pulsadores);
			xSemaphoreTake(S_pulsador, portMAX_DELAY);
			// si se libera el semaforo, se apretó algo:
			xQueueReceive(Q_pulsadoresEstado, &inputUserReceived, portMAX_DELAY);

			// identifica las posiciones dentro del menú principal y selección:
			MENU_postStage = MENU_posicionamiento(inputUserReceived);
			if(MENU_postStage == MENU_previousStage)
			{
				// seleccionó lo que sea que estaba remarcado:
				switch(MENU_postStage)
				{
				case UP_LEFT:
					// espectro analisis:
					MdE = 4;
					break;
				case UP_MIDDLE:
					// usb enviar datos:
					MdE = 10;
					break;
				case DOWN_MIDDLE:
					// opciones:
					MdE = 8;
					break;
				default:
					break;
				}
			}
			else
			{
				// si no, me lo guardo y reviso la prox:
				MENU_previousStage = MENU_postStage;
			}
			// una vez borrado el marco actual e impreso el nuevo, aviso a pulsador:
			inputUserReceived.action_pending=false;
			xQueueSendToBack(Q_pulsadoresEstado, &inputUserReceived, portMAX_DELAY);
			xSemaphoreGive(S_MdE_pulsador);
			break;
		case 4:
			// se clickeó en el menú principal la selección sobre el espectro:
			// cargo la diapositiva base del espectro:

			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			// Espero que esté sin usar la SD:
			xSemaphoreTake(S_SD, portMAX_DELAY);

			ILI9341_action.petition = 1;
			ILI9341_action.rotation = 2;
			ILI9341_action.imageSelected = ESPECTRO;
			SD_action.imageSelected = ESPECTRO;
			SD_action.petition = 1;
			xQueueSendToBack(Q_SD, &SD_action, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			MdE = 5;
			break;
		case 5:
			// espectro cargado, inicio conversión:

			// Espero que esté sin usar la SD:
			xSemaphoreTake(S_SD, portMAX_DELAY);

			SD_action.petition = 2; // cargar Espectro Color
			xQueueSendToBack(Q_SD, &SD_action, portMAX_DELAY);

			// Espero que esté sin usar la SD:
			xSemaphoreTake(S_SD, portMAX_DELAY);
			SD_action.petition = 0xFF; // mando default para liberar semaforo
			xQueueSendToBack(Q_SD, &SD_action, portMAX_DELAY);

			ILI9341_action.petition = 7; // cargar Espectro Color
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);

			// cambio a interacción sobre diapositiva espectro mientras carga:
			userInteraction_t aux = inputUserReceived;
			aux.input_activated = IDLE;
			ESPECTRO_posicionamiento(aux); // otra forma de poner la selección

			intensity_saved = false; // se pidio otro espectrograma

			MdE = 6;
			break;
		case 6:
			// espero interactuar con el usuario en diapositiva espectro:
			xSemaphoreTake(S_pulsador, portMAX_DELAY);
			// si se libera el semaforo, se apretó algo:
			xQueueReceive(Q_pulsadoresEstado, &inputUserReceived, portMAX_DELAY);

			// identifica las posiciones dentro del menú principal y selección:
			ESPECTRO_postStage = ESPECTRO_posicionamiento(inputUserReceived);
			if((ESPECTRO_postStage == ESPECTRO_previousStage) && (inputUserReceived.input_activated == SELECTION))
			{
				// seleccionó lo que sea que estaba remarcado:
				switch(ESPECTRO_postStage)
				{
				case DOWN_LEFT:
					// guardar espectrograma actual:
					intensity_saved = true;
					MdE = 7;
					break;
				case DOWN_RIGHT:
					// volver al menu principal:
					MdE = 2;
					break;
				case DOWN_MIDDLE:
					// opciones:
					MdE = 8;
					break;
				default:
					break;
				}
			}
			else
			{
				// si no, me lo guardo y reviso la prox:
				ESPECTRO_previousStage = ESPECTRO_postStage;
			}
			// una vez borrado el marco actual e impreso el nuevo, aviso a pulsador:
			inputUserReceived.action_pending=false;
			xQueueSendToBack(Q_pulsadoresEstado, &inputUserReceived, portMAX_DELAY);
			xSemaphoreGive(S_MdE_pulsador);
			break;
		case 7:
			// muestro un dialogo que indique que se guardó y vuelvo al menú:

			ILI9341_action.petition = 8; // cargar pop up en Espectro_saved
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			vTaskDelay(2000);
			MdE = 2;
			break;
		case 8:
			// cargar opciones:
			ILI9341_action.petition = 9; // cargar opciones
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			MdE = 9;

			userInteraction_t aux3 = inputUserReceived;
			aux3.input_activated = IDLE;
			OPCIONES_posicionamiento(aux3); // otra forma de poner la selección
			break;
		case 9:
			// opciones:
			// espero interactuar con el usuario en opciones:
			xSemaphoreTake(S_pulsador, portMAX_DELAY);
			// si se libera el semaforo, se apretó algo:
			xQueueReceive(Q_pulsadoresEstado, &inputUserReceived, portMAX_DELAY);

			// identifica las posiciones dentro del menú principal y selección:
			OPCIONES_postStage = OPCIONES_posicionamiento(inputUserReceived);
			if((OPCIONES_postStage == OPCIONES_previousStage) && (inputUserReceived.input_activated == SELECTION))
			{
				// seleccionó lo que sea que estaba remarcado:
				switch(OPCIONES_postStage)
				{
				case DOWN_LEFT:
					// volver al menu:
					MdE = 2;
					break;
				default:
					break;
				}
			}
			else
			{
				// si no, me lo guardo y reviso la prox:
				OPCIONES_previousStage = OPCIONES_postStage;
			}
			// una vez borrado el marco actual e impreso el nuevo, aviso a pulsador:
			inputUserReceived.action_pending=false;
			xQueueSendToBack(Q_pulsadoresEstado, &inputUserReceived, portMAX_DELAY);
			xSemaphoreGive(S_MdE_pulsador);
			break;
		case 10:
			// se clickeó en el menú principal la opción de subir datos por USB:
			// checkeo si hay datos que enviar:
			if(intensity_saved)
			{
				// paso a iniciar el proceso de espera:
				MdE = 11;
			}
			else
			{
				// no hay datos guardados, pop up y al menú:
				MdE = 12;
			}
			break;
		case 11:
			// cargo pantalla de espera de recepcion USB con timeout, luego espero:

			ILI9341_action.petition = 11; // recepcion USB espera
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			MdE = 13;
			break;
		case 12:
			// muestro un dialogo diciendo no hay datos y vuelvo al menú:

			ILI9341_action.petition = 10; // cargar pop up en USB no hay datos
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			vTaskDelay(2000);
			MdE = 2;
			break;
		case 13:
			// cargo timer timeout, reviso usb y voy actualizando el timer:
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			ILI9341_action.petition = 0xFF; // libero el semaforo
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);

			timeout = HAL_GetTick() + 5000;
			int8_t auxCounter = 4;
			do
			{
				USB_task();

				if(flag == 1)
				{
					break;
				}

				int32_t auxTime = timeout - HAL_GetTick();

				if(((auxTime/1000) < auxCounter) && (auxTime != 5000))
				{
					// pasó 1 segundo:
					auxCounter --;
					ILI9341_action.petition = 12; // decrementar timer y actualizar
					ILI9341_action.rotation = 2;
					// Espero que esté libre la pantalla:
					xSemaphoreTake(S_ILI9341, portMAX_DELAY);
					xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
				}
			}while(HAL_GetTick() < timeout);
			if(flag != 1)
			{
				MdE = 2; // no llegó el dato o fue erroneo, vuelvo al menu.
			}
			else
			{
				MdE = 14; // llegó la peticion desde PC
			}
			break;
		case 14:
			// llegó petición, procedo a mandar los datos:
			ILI9341_action.petition = 13; // muestro que se mandan los datos.
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);

			int8_t error = USB_task();

			if(error == 0)
				MdE = 15;
			if(error == -1)
				MdE = 16;
			break;
		case 15:
			// muestro que se terminaron de enviar los datos y vuelvo al menu:
			ILI9341_action.petition = 14; // muestro que se terminó.
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			vTaskDelay(2000);
			MdE = 2;
			break;
		case 16:
			// muestro que hubo un error en la comunicacion usb y vuelvo al menu:
			ILI9341_action.petition = 15; // muestro el error.
			ILI9341_action.rotation = 2;
			// Espero que esté libre la pantalla:
			xSemaphoreTake(S_ILI9341, portMAX_DELAY);
			xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY);
			vTaskDelay(2000);
			MdE = 2;
			break;
		default:
			break;
		}
	}
}

void T_detectar_pulsador(void* arg)
{
	static userInteraction_t inputUser;

	inputUser.actions = 0;
	inputUser.action_pending = 0;
	inputUser.pulsed = 0;

	// espero inicialización:
	xSemaphoreTake(S_MdE_pulsador, portMAX_DELAY);

	while(1)
	{
		if(inputUser.action_pending)
		{
			//xSemaphoreTake(S_MdE_pulsador, portMAX_DELAY);
			xQueueReceive(Q_pulsadoresEstado, &inputUser, portMAX_DELAY);
			continue;
		}
		if(inputUser.pulsed) // se presionó un botón y se tiene que soltar:
		{
			switch(inputUser.input_activated)
			{
			case MOVE_UP:
				if(HAL_GPIO_ReadPin(BOTON_UP_GPIO_Port, BOTON_UP_Pin) == GPIO_PIN_RESET)
				{
					inputUser.pulsed = false; // se soltó el botón
					inputUser.input_activated = IDLE;
				}
				break;
			case MOVE_DOWN:
				if(HAL_GPIO_ReadPin(BOTON_DOWN_GPIO_Port, BOTON_DOWN_Pin) == GPIO_PIN_RESET)
				{
					inputUser.pulsed = false; // se soltó el botón
					inputUser.input_activated = IDLE;
				}
				break;
			case MOVE_LEFT:
				if(HAL_GPIO_ReadPin(BOTON_LEFT_GPIO_Port, BOTON_LEFT_Pin) == GPIO_PIN_RESET)
				{
					inputUser.pulsed = false; // se soltó el botón
					inputUser.input_activated = IDLE;
				}
				break;
			case MOVE_RIGHT:
				if(HAL_GPIO_ReadPin(BOTON_RIGHT_GPIO_Port, BOTON_RIGHT_Pin) == GPIO_PIN_RESET)
				{
					inputUser.pulsed = false; // se soltó el botón
					inputUser.input_activated = IDLE;
				}
				break;
			case SELECTION:
				if(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)
				{
					inputUser.pulsed = false; // se soltó el botón
					inputUser.input_activated = IDLE;
				}
				break;
			default:
				break;
			}
			continue;
		}
		switch(inputUser.inputs)
		{
		case MOVE_RIGHT:

			// MOVE_RIGHT:
			if(HAL_GPIO_ReadPin(BOTON_RIGHT_GPIO_Port, BOTON_RIGHT_Pin) == GPIO_PIN_SET)
			{
				vTaskDelay(150);

				if(HAL_GPIO_ReadPin(BOTON_RIGHT_GPIO_Port, BOTON_RIGHT_Pin) == GPIO_PIN_SET)
				{
					// después del timeout, sigue pulsado --> se pulsó
					inputUser.pulsed = true;
					inputUser.action_pending = true;
					inputUser.input_activated = MOVE_RIGHT;

					xQueueSendToBack(Q_pulsadoresEstado, &inputUser, portMAX_DELAY);
					xSemaphoreGive(S_pulsador);
				}
			}

			inputUser.inputs = MOVE_LEFT;
			break;
		case MOVE_LEFT:
			// checkeo BOTON_LEFT:
			if(HAL_GPIO_ReadPin(BOTON_LEFT_GPIO_Port, BOTON_LEFT_Pin) == GPIO_PIN_SET)
			{
				vTaskDelay(150);

				if(HAL_GPIO_ReadPin(BOTON_LEFT_GPIO_Port, BOTON_LEFT_Pin) == GPIO_PIN_SET)
				{
					// después del timeout, sigue pulsado --> se pulsó
					inputUser.pulsed = true;
					inputUser.action_pending = true;
					inputUser.input_activated = MOVE_LEFT;

					xQueueSendToBack(Q_pulsadoresEstado, &inputUser, portMAX_DELAY);
					xSemaphoreGive(S_pulsador);
				}
			}
			inputUser.inputs = MOVE_UP;
			break;
		case MOVE_UP:
			// checkeo BOTON_UP:
			if(HAL_GPIO_ReadPin(BOTON_UP_GPIO_Port, BOTON_UP_Pin) == GPIO_PIN_SET)
			{
				vTaskDelay(150);

				if(HAL_GPIO_ReadPin(BOTON_UP_GPIO_Port, BOTON_UP_Pin) == GPIO_PIN_SET)
				{
					// después del timeout, sigue pulsado --> se pulsó
					inputUser.pulsed = true;
					inputUser.action_pending = true;
					inputUser.input_activated = MOVE_UP;

					xQueueSendToBack(Q_pulsadoresEstado, &inputUser, portMAX_DELAY);
					xSemaphoreGive(S_pulsador);
				}
			}
			inputUser.inputs = MOVE_DOWN;
			break;
		case MOVE_DOWN:
			// checkeo BOTON_DOWN:
			if(HAL_GPIO_ReadPin(BOTON_DOWN_GPIO_Port, BOTON_DOWN_Pin) == GPIO_PIN_SET)
			{
				vTaskDelay(150);

				if(HAL_GPIO_ReadPin(BOTON_DOWN_GPIO_Port, BOTON_DOWN_Pin) == GPIO_PIN_SET)
				{
					// después del timeout, sigue pulsado --> se pulsó
					inputUser.pulsed = true;
					inputUser.action_pending = true;
					inputUser.input_activated = MOVE_DOWN;

					xQueueSendToBack(Q_pulsadoresEstado, &inputUser, portMAX_DELAY);
					xSemaphoreGive(S_pulsador);
				}
			}
			inputUser.inputs = SELECTION;
			break;
		case SELECTION:
			// checkeo selección:
			if(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET)
			{
				vTaskDelay(150);

				if(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET)
				{
					// después del timeout, sigue pulsado --> se pulsó
					inputUser.pulsed = true;
					inputUser.action_pending = true;
					inputUser.input_activated = SELECTION;

					xQueueSendToBack(Q_pulsadoresEstado, &inputUser, portMAX_DELAY);
					xSemaphoreGive(S_pulsador);
				}
			}
			inputUser.inputs = MOVE_RIGHT;
			break;
		default:
			break;
		}
	}
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//extern const uint16_t logo_UTN;
//extern const uint16_t menu_principal1;


// largo imagen 76800*2 bytes = 153600
// de cada unidad de vector saco 2 bytes
// osea 512 bytes los tengo en 256 ciclos
/*
uint8_t write_image(const uint16_t* image)
{
	uint8_t error=0;
	shorty_bytes_t image_unit;
	uint32_t j = 0;
	uint32_t blockIndex=0;
	uint32_t imageIndex=0;
	uint32_t readCheck=0;

	for(blockIndex=0; blockIndex < 300; blockIndex++)
	{
		  for(int i = 0; i < 256; i++)
		  {
			  image_unit.shorty = image[i+imageIndex];
			  bufferTx[j]=image_unit.byte_MSB;
			  bufferTx[j+1]=image_unit.byte_LSB;
			  j++;
			  j++;
		  }
		  j=0;
		  imageIndex += 256;

		  sd_read_block(FIRST_BLOCK+blockIndex,bufferRx);

		  for(uint32_t i=0; i < 512; i++)
		  {
			  if(bufferRx[i] == bufferTx[i])
			  {
				  readCheck++;
			  }
		  }

		  if(readCheck == 512)
		  {
			  // lo que quiero escribir ya está escrito
			  readCheck=0;
		  }
		  else
		  {
			  // escribo el bloque:
			  readCheck=0;
			  error = sd_write_block(FIRST_BLOCK+blockIndex, bufferTx);
			  if(error == 0)
			  {
				  return 1;
			  }
		  }
	}

	  return 0;
}
*/

/*
uint8_t read_image(const uint16_t* image)
{
	uint8_t error=0;
	shorty_bytes_t image_unit;
	uint32_t j = 0;
	uint32_t blockIndex=0;
	uint32_t imageIndex=0;
	uint32_t readCheck=0;
	uint32_t successes=0;

	for(blockIndex=0; blockIndex < 300; blockIndex++)
	{
		  for(int i = 0; i < 256; i++)
		  {
			  image_unit.shorty = image[i+imageIndex];
			  bufferTx[j]=image_unit.byte_MSB;
			  bufferTx[j+1]=image_unit.byte_LSB;
			  j++;
			  j++;
		  }
		  j=0;
		  imageIndex += 256;

		  sd_read_block(FIRST_BLOCK+blockIndex,bufferRx);

		  for(uint32_t i=0; i < 512; i++)
		  {
			  if(bufferRx[i] == bufferTx[i])
			  {
				  readCheck++;
			  }
		  }

		  if(readCheck == 512)
		  {
			  // lo que quiero leer está escrito

			  readCheck=0;
			  successes++;
		  }
	}
	if(successes != 300)
	{
		error = 1;
	}

	return error;
}
*/

void MENU_desplazamiento_print(menuPosition_t positionReceived, bool isSelected)
{
	static uint32_t x=0;
	static uint32_t y=0;
	uint16_t color = 0;

	if(isSelected)
	{
		color = COLOR_ORANGE;
	}
	else
	{
		color = COLOR_BLUE;
	}
	switch(positionReceived)
	{
	case UP_LEFT:
		x = 0;
		y = 0;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, color); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	case UP_MIDDLE:
		x = 99;
		y = 0;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, color); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	case UP_RIGHT:
		x = 99 * 2;
		y = 0;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, color); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	case DOWN_LEFT:
		x = 0;
		y = 130;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, color); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	case DOWN_MIDDLE:
		x = 99;
		y = 130;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, color); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	case DOWN_RIGHT:
		x = 99 * 2;
		y = 130;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, color); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	default:
		break;
	}
}

void MENU_desplazamiento_erase(menuPosition_t positionReceived)
{
	static uint32_t x=0;
	static uint32_t y=0;
	switch(positionReceived)
	{
	case UP_LEFT:
		x = 0;
		y = 0;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, COLOR_WHITE); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	case UP_MIDDLE:
		x = 99;
		y = 0;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, COLOR_WHITE); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	case UP_RIGHT:
		x = 99 * 2;
		y = 0;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, COLOR_WHITE); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	case DOWN_LEFT:
		x = 0;
		y = 130;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, COLOR_WHITE); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	case DOWN_MIDDLE:
		x = 99;
		y = 130;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, COLOR_WHITE); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	case DOWN_RIGHT:
		x = 99 * 2;
		y = 130;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(10+x, 91+y, 109+x, 96+y, COLOR_WHITE); // lado inferior
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	default:
		break;
	}
}
/*
 * brief: Se encarga de manejar el posicionamiento y selección dentro del menú principal
 * param inputUserReceived: el botón que haya apretado el usuario.
 * ret: menuPosition_t:
 * 		Define la posición actual en que se encuentra el menú.
 * 		De iniciar y terminar en la misma posición, asuma selección.
 *
 */
menuPosition_t MENU_posicionamiento(userInteraction_t inputUserReceived)
{
	static menuPosition_t posicion=UP_LEFT;
	bool isSelected = false;
	T_ILI9341_action_t ILI9341_action;

	ILI9341_action.petition = 3; // MENU_desplazamiento_erase(posicion);
	ILI9341_action.positionReceived = posicion;
	xSemaphoreTake(S_ILI9341, portMAX_DELAY); // pantalla libre:
	xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY); // directiva

	switch(posicion)
	{
	case UP_LEFT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = UP_MIDDLE;
			break;
		case MOVE_LEFT:
			posicion = UP_RIGHT;
			break;
		case MOVE_UP:
			posicion = DOWN_LEFT;
			break;
		case MOVE_DOWN:
			posicion = DOWN_LEFT;
			break;
		case SELECTION:
			isSelected = true;
			break;
		default:
			break;
		}
		break;
	case UP_MIDDLE:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = UP_RIGHT;
			break;
		case MOVE_LEFT:
			posicion = UP_LEFT;
			break;
		case MOVE_UP:
			posicion = DOWN_MIDDLE;
			break;
		case MOVE_DOWN:
			posicion = DOWN_MIDDLE;
			break;
		case SELECTION:
			isSelected = true;
			break;
		default:
			break;
		}
		break;
	case UP_RIGHT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = UP_LEFT;
			break;
		case MOVE_LEFT:
			posicion = UP_MIDDLE;
			break;
		case MOVE_UP:
			posicion = DOWN_RIGHT;
			break;
		case MOVE_DOWN:
			posicion = DOWN_RIGHT;
			break;
		case SELECTION:
			isSelected = true;
			break;
		default:
			break;
		}
		break;
	case DOWN_LEFT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = DOWN_MIDDLE;
			break;
		case MOVE_LEFT:
			posicion = DOWN_RIGHT;
			break;
		case MOVE_UP:
			posicion = UP_LEFT;
			break;
		case MOVE_DOWN:
			posicion = UP_LEFT;
			break;
		case SELECTION:
			isSelected = true;
			break;
		default:
			break;
		}
		break;
	case DOWN_MIDDLE:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = DOWN_RIGHT;
			break;
		case MOVE_LEFT:
			posicion = DOWN_LEFT;
			break;
		case MOVE_UP:
			posicion = UP_MIDDLE;
			break;
		case MOVE_DOWN:
			posicion = UP_MIDDLE;
			break;
		case SELECTION:
			isSelected = true;
			break;
		default:
			break;
		}
		break;
	case DOWN_RIGHT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = DOWN_LEFT;
			break;
		case MOVE_LEFT:
			posicion = DOWN_MIDDLE;
			break;
		case MOVE_UP:
			posicion = UP_RIGHT;
			break;
		case MOVE_DOWN:
			posicion = UP_RIGHT;
			break;
		case SELECTION:
			isSelected = true;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	ILI9341_action.petition = 4; // MENU_desplazamiento_print(posicion, isSelected);
	ILI9341_action.positionReceived = posicion;
	ILI9341_action.MENU_isSelected = isSelected;
	xSemaphoreTake(S_ILI9341, portMAX_DELAY); // pantalla libre:
	xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY); // directiva
	return posicion;
}

menuPosition_t ESPECTRO_posicionamiento(userInteraction_t inputUserReceived)
{
	static menuPosition_t posicion = DOWN_LEFT;
	bool isSelected = false;
	T_ILI9341_action_t ILI9341_action;

	ILI9341_action.petition = 5; // ESPECTRO_desplazamiento_erase(posicion);
	ILI9341_action.positionReceived = posicion;
	xSemaphoreTake(S_ILI9341, portMAX_DELAY); // pantalla libre:
	xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY); // directiva
	switch(posicion)
	{
	case DOWN_LEFT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = DOWN_MIDDLE;
			break;
		case MOVE_LEFT:
			posicion = DOWN_RIGHT;
			break;
		case SELECTION:
			isSelected = true;
			break;
		default:
			break;
		}
		break;
	case DOWN_MIDDLE:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = DOWN_RIGHT;
			break;
		case MOVE_LEFT:
			posicion = DOWN_LEFT;
			break;
		case SELECTION:
			isSelected = true;
		default:
			break;
		}
		break;
	case DOWN_RIGHT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_RIGHT:
			posicion = DOWN_LEFT;
			break;
		case MOVE_LEFT:
			posicion = DOWN_MIDDLE;
			break;
		case SELECTION:
			isSelected = true;
		default:
			break;
		}
		break;
	default:
		break;
	}
	ILI9341_action.petition = 6; // ESPECTRO_desplazamiento_print(posicion, isSelected);
	ILI9341_action.positionReceived = posicion;
	ILI9341_action.MENU_isSelected = isSelected;
	xSemaphoreTake(S_ILI9341, portMAX_DELAY); // pantalla libre:
	xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY); // directiva
	return posicion;
}

void ESPECTRO_desplazamiento_erase(menuPosition_t positionReceived)
{
	static uint32_t x=0;
	static uint32_t y=0;
	switch(positionReceived)
	{
	case DOWN_LEFT:
		x = 0;
		y = 150;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	case DOWN_MIDDLE:
		x = 99;
		y = 150;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	case DOWN_RIGHT:
		x = 99 * 2;
		y = 150;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, COLOR_WHITE); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, COLOR_WHITE); // lado izquierdo
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, COLOR_WHITE); // lado derecho
		break;
	default:
		break;
	}
}

void ESPECTRO_desplazamiento_print(menuPosition_t positionReceived, bool isSelected)
{
	static uint32_t x=0;
	static uint32_t y=0;
	uint16_t color = 0;

	if(isSelected)
	{
		color = COLOR_ORANGE;
	}
	else
	{
		color = COLOR_BLUE;
	}
	switch(positionReceived)
	{
	case DOWN_LEFT:
		x = 0;
		y = 150;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	case DOWN_MIDDLE:
		x = 99;
		y = 150;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	case DOWN_RIGHT:
		x = 99 * 2;
		y = 150;
		ILI9341.fillRectangle(10+x, 10+y, 109+x, 15+y, color); // lado superior
		ILI9341.fillRectangle(10+x, 10+y, 15+x, 96+y, color); // lado izquierdo
		ILI9341.fillRectangle(104+x, 10+y, 109+x, 96+y, color); // lado derecho
		break;
	default:
		break;
	}
}

// gausianna para sensor:
float gauss(float x, float mean, float sigma)
{
	float dx = x - mean;
	return expf(-(dx * dx) / (2.0f * sigma * sigma));
}

// intensidad según lambda:
float intensity_from_wavelength(float lambda, float Rn, float Gn, float Bn)
{
	float A = gauss(lambda, 465.0f, 20.0f); // estimación para azul
	float G = gauss(lambda, 525.0f, 30.0f); // estimación para verde
	float R = gauss(lambda, 615.0f, 40.0f); // estimación para rojo

	float I = Bn * A + Gn * G + Rn * R;

	// normalizo:
	if(I < 0.0){ I = 0.0; }
	if(I > 1.0){ I = 1.0; }

	return I;
}

void ESPECTRO_algoritmoIntensidadEspectro(void)
{
	uint16_t r, g, b, c;

	Adafruit_TCS34725_getRawData(&r, &g, &b, &c);

	bool auxC1 = (c < r && c < g) ? true : false;
	bool auxC2 = (c < r && c < b) ? true : false;
	bool auxC3 = (c < g && c < b) ? true : false;

	if(auxC1 || auxC2 || auxC3)
	{
		ILI9341.fillRectangle(20, 50, 280, 150, COLOR_MAGENTA);
		ILI9341.printText("SENSOR OVERFLOW", 25,  70, COLOR_BLACK, COLOR_MAGENTA, 2);
		ILI9341.printText("INTENTE NUEVAMENTE", 25,  100, COLOR_BLACK, COLOR_MAGENTA, 2);
		ILI9341.printText("ALEJESE DE LA FUENTE", 25,  130, COLOR_BLACK, COLOR_MAGENTA, 2);
		return;
	}

	// Normalización y protección contra dividir por 0:
	float Rn = (c == 0) ? 0 : ((float)r / (float)c);
	float Gn = (c == 0) ? 0 : ((float)g / (float)c);
	float Bn = (c == 0) ? 0 : ((float)b / (float)c);

	// chequeamos cuál tiene el valor mayor y lo usamos como referencia:
	float maxRGB = fmaxf(Rn, fmaxf(Gn, Bn));
	if(maxRGB > 1.0f)
	{
		Rn /= maxRGB;
		Gn /= maxRGB;
		Bn /= maxRGB;
	}

	// toca mostrar en pantalla:
	for(uint32_t x = 0; x < 296; x++)
	{
		// defino mi longitud de onda en base a mi piso, mi ancho de banda y mi posición actual:
		float lambda = 380.0f + (float)x * (750.0f - 380.0f) / 295.0f;
		float I = intensity_from_wavelength(lambda, Rn, Gn, Bn);
		int32_t y = 130 - (int32_t)(I * 100.0f); // altura de intensidad de 100px
		intensity_I_measured[x] = I;
		while(y <= Espectro_y2)
		{
			uint16_t pixel = (((bufferEspectro[ (y-Espectro_y1) * 296 + x]) & 0xFF00) >> 8) | (((bufferEspectro[ (y-Espectro_y1) * 296 + x]) & 0x00FF) << 8);
			ILI9341.drawPixel(Espectro_x1 + x, y, pixel);
			y++;
		}
	}
}

void ESPECTRO_cargarEspectroColor(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, image_select_t imageSelected)
{
	uint16_t width;	// ancho espectro 296
	//uint16_t height;	// altura espectro 101
	uint32_t EspectroBufferIndex = 0;
	uint32_t firstPixel, firstByte, block, offset;
	int32_t bytesNeeded, bytesAvailable, bytesTakenFromRow;
	uint32_t lastBlockLoaded = 301;	// valor imposible a propósito

	width = x2 - x1 + 1;
	//height = y2 - y1 + 1;

	for(uint16_t y = y1; y <= y2; y++)
	{
		// Es más fácil rescatar por fila:
		firstPixel = y * 320 + x1;

		firstByte = firstPixel * 2; // uint16 vs uint8
		block = firstByte / 512; // bloque donde se encuentra
		offset = firstByte % 512; // offset desde el bloque donde se encuentra
		bytesNeeded = width * 2; // 296 * 2 = 592 bytes, más de 1 bloque.

		while(bytesNeeded > 0)
		{
			// me encargo de la carga de más de 1 bloque:
			if(block != lastBlockLoaded)
			{
				// TODO: podría necesitar checkeo de errores.
				sd_read_block(imageSelected + block, dummyBufferTx, bufferRx);
				lastBlockLoaded = block;
			}

			bytesAvailable = 512 - offset;
			bytesTakenFromRow = (bytesNeeded < bytesAvailable) ? bytesNeeded : bytesAvailable; // cantidad de bytes que copio

			memcpy(&(bufferEspectro[EspectroBufferIndex]), &(bufferRx[offset]), bytesTakenFromRow);

			EspectroBufferIndex += bytesTakenFromRow / 2; // uint16 vs uint8
			bytesNeeded -= bytesTakenFromRow; // si me faltan, reitera
			block++;
			offset = 0;
		}
	}
}

menuPosition_t OPCIONES_posicionamiento(userInteraction_t inputUserReceived)
{
	static menuPosition_t posicion = UP_LEFT;
	bool isSelected = false;
	T_ILI9341_action_t ILI9341_action;

	ILI9341_action.dataVar = 0; // nothing to do

	ILI9341_action.petition = 16; // OPCIONES_desplazamiento_erase
	ILI9341_action.positionReceived = posicion;
	xSemaphoreTake(S_ILI9341, portMAX_DELAY); // pantalla libre:
	xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY); // directiva
	switch(posicion)
	{
	case UP_LEFT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_DOWN:
			posicion = UP_MIDDLE;
			break;
		case SELECTION:
			isSelected = true;
			ILI9341_action.dataVar = 1; // show next integration time
			break;
		default:
			break;
		}
		break;
	case UP_MIDDLE:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_UP:
			posicion = UP_LEFT;
			break;
		case MOVE_DOWN:
			posicion = UP_RIGHT;
			break;
		case SELECTION:
			isSelected = true;
			ILI9341_action.dataVar = 2; // show next gain option
		default:
			break;
		}
		break;
	case UP_RIGHT:
		switch(inputUserReceived.input_activated)
		{
		case MOVE_UP:
			posicion = UP_MIDDLE;
			break;
		case MOVE_DOWN:
			posicion = DOWN_LEFT;
			break;
		case SELECTION:
			isSelected = true;
			ILI9341_action.dataVar = 3; // apply changes
		default:
			break;
		}
		break;
		case DOWN_LEFT:
			switch(inputUserReceived.input_activated)
			{
			case MOVE_UP:
				posicion = UP_RIGHT;
				break;
			case SELECTION:
				isSelected = true;
				ILI9341_action.dataVar = 4; // go back to menu
			default:
				break;
			}
			break;
	default:
		break;
	}
	ILI9341_action.petition = 17; // OPCIONES_desplazamiento_print
	ILI9341_action.positionReceived = posicion;
	ILI9341_action.MENU_isSelected = isSelected;
	xSemaphoreTake(S_ILI9341, portMAX_DELAY); // pantalla libre:
	xQueueSendToBack(Q_ILI9341, &ILI9341_action, portMAX_DELAY); // directiva
	return posicion;
}

void OPCIONES_desplazamiento_print(menuPosition_t positionReceived, bool isSelected, char* integrationTimeText, char* gainText)
{
	uint16_t color = COLOR_WHITE;
	uint16_t color_background = COLOR_MAGENTA;

	switch(positionReceived)
	{
	case UP_LEFT:
		ILI9341.printText("TIEMPO INTEGRACION:", 5,  45, color, color_background, 2);
		ILI9341.printText(integrationTimeText, 230,  45, color, color_background, 2);
		break;
	case UP_MIDDLE:
		ILI9341.printText("GANANCIA:", 5, 75, color, color_background, 2);
		ILI9341.printText(gainText, 230, 75, color, color_background, 2);
		break;
	case UP_RIGHT:
		ILI9341.printText("APLICAR CAMBIOS", 5, 135, color, color_background, 2);
		break;
	case DOWN_LEFT:
		ILI9341.printText("VOLVER AL MENU", 5, 165, color, color_background, 2);
		break;
	default:
		break;
	}
}

void OPCIONES_desplazamiento_erase(menuPosition_t positionReceived, char* integrationTimeText, char* gainText)
{
	switch(positionReceived)
	{
	case UP_LEFT:
		ILI9341.printText("TIEMPO INTEGRACION:", 5,  45, COLOR_BLACK, COLOR_YELLOW, 2);
		ILI9341.printText(integrationTimeText, 230,  45, COLOR_BLACK, COLOR_YELLOW, 2);
		break;
	case UP_MIDDLE:
		ILI9341.printText("GANANCIA:", 5, 75, COLOR_BLACK, COLOR_YELLOW, 2);
		ILI9341.printText(gainText, 230, 75, COLOR_BLACK, COLOR_YELLOW, 2);
		break;
	case UP_RIGHT:
		ILI9341.printText("APLICAR CAMBIOS", 5, 135, COLOR_BLACK, COLOR_YELLOW, 2);
		break;
	case DOWN_LEFT:
		ILI9341.printText("VOLVER AL MENU", 5, 165, COLOR_BLACK, COLOR_YELLOW, 2);
		break;
	default:
		break;
	}
}

void speed_up_SPI(void)
{
	__HAL_SPI_DISABLE(&hspi1);
	//lcdSPIhandle->Init.DataSize = SPI_DATASIZE_16BIT;
	//hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
	HAL_SPI_Init(&hspi1);

	__HAL_SPI_DISABLE(&hspi2);
	//lcdSPIhandle->Init.DataSize = SPI_DATASIZE_16BIT;
	//hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	HAL_SPI_Init(&hspi2);

	HAL_Delay(10);

}

int8_t USB_task(void)
{
	uint32_t sent_bytes = 0;
	  if (flag_rx == 1)
	  {
		  //Check if the first byte of the report buffer equals 1
		  if (report_buffer[0] == 1 || report_buffer[1] == 1)
		  {
			  //Turn the user LED on
			  Adafruit_TCS34725_setInterrupt(false); // turn on LED
			  //Turn the user LED off
			  Adafruit_TCS34725_setInterrupt(true); // turn off LED

			  flag = 1; // me llegó de recepción el pedido de PC de enviar tramas.
		  }
		  flag_rx = 0;
		  return 0;
	  }
	  //To send the output data when the button is pressed on PC:
	  if (flag==1)
	  {
		  sent_bytes = 0;
		  uint32_t float_index = 0;
		  while(sent_bytes < (296 * sizeof(float)))
		  {
			  uint8_t USBD_state = USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, (uint8_t*)&(intensity_I_measured[float_index]), 64);
			  //vTaskDelay(10);
			  if(USBD_state == USBD_OK)
			  {
				  sent_bytes += 64;
				  float_index += (64/sizeof(float));
			  }
			  if(USBD_state == USBD_BUSY)
				  continue;
			  if(USBD_state == USBD_FAIL)
			  {
				  return -1;
			  }
		  }
		  flag = 0;
		  return 0;
	  }
	  return 2;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
	BaseType_t* xHigherPriorityTaskWoken = pdFALSE;

	if(hspi->Instance == SPI1)	// ILI9341
	{
		// descomentar para identificar errores:
		//volatile HAL_SPI_StateTypeDef st = hspi->State;
		//volatile uint32_t err = HAL_SPI_GetError(hspi);
		xSemaphoreGiveFromISR(S_ILI9341_ISR, xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	BaseType_t* xHigherPriorityTaskWoken = pdFALSE;

	if(hspi->Instance == SPI2)	// SD
	{
		// descomentar para identificar errores:
		//volatile HAL_SPI_StateTypeDef st = hspi->State;
		//volatile uint32_t err = HAL_SPI_GetError(hspi);
		xSemaphoreGiveFromISR(S_SD_ISR, xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */


  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_SPI2_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_I2C3_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(SD_SPI_CS_GPIO_Port, SD_SPI_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_SPI_CS_GPIO_Port, TFT_SPI_CS_Pin, GPIO_PIN_SET);

  HAL_Delay(10);

  HAL_TIM_Base_Start(&htim3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

  ILI9341_Class_Init(&ILI9341, &ILI9341_HW);

  uint8_t success = 0;

  // inicializo variables para SD:
  SD_SPI_HANDLE = &hspi2;
  SD_CS_PIN = SD_SPI_CS_Pin;
  SD_CS_PORT = SD_SPI_CS_GPIO_Port;

  success = sd_init();

  if(!success)
  {
	  //SD fallo init
	  Error_Handler();
  }
  HAL_Delay(10);

  	HAL_Delay(10);

  	success = 0;

  	AS726X_I2C_HANDLE = &hi2c3;
  	AS726X_init(&AS7262);

  	Wire_begin();

  	HAL_Delay(10);

  	Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_2_4MS, TCS34725_GAIN_4X);
  	success = Adafruit_TCS34725_begin();
    if(!success)
    {
  	  //TCS34725 fallo init
  	  Error_Handler();
    }

    Adafruit_TCS34725_setInterrupt(true); // turn off LED

	speed_up_SPI();

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */

	S_pulsador = xSemaphoreCreateBinary();
	S_MdE_pulsador = xSemaphoreCreateBinary();
	S_ILI9341 = xSemaphoreCreateBinary();
	S_ILI9341_ISR = xSemaphoreCreateBinary();
	S_SD = xSemaphoreCreateBinary();
	S_SD_ISR = xSemaphoreCreateBinary();

	xSemaphoreTake(S_MdE_pulsador, 0);;
	xSemaphoreTake(S_pulsador, 0);
	xSemaphoreTake(S_ILI9341, 0);
	xSemaphoreTake(S_ILI9341_ISR, 0);
	xSemaphoreTake(S_SD, 0);
	xSemaphoreTake(S_SD_ISR, 0);

  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */

	Q_pulsadoresEstado = xQueueCreate( 1, sizeof(userInteraction_t) );
	Q_ILI9341 = xQueueCreate( 1, sizeof(T_ILI9341_action_t) );
	Q_SD = xQueueCreate( 1, sizeof(T_SD_action_t) );


  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 256);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

	xTaskCreate(T_MdE, "T_MaquinaDeEstados", configMINIMAL_STACK_SIZE+128, NULL/*parametros*/, tskIDLE_PRIORITY+2, NULL /*handler*/);

	xTaskCreate(T_ILI9341, "T_ILI9341_SPI", configMINIMAL_STACK_SIZE+128, NULL/*parametros*/, tskIDLE_PRIORITY+2, NULL /*handler*/);

	xTaskCreate(T_SD, "T_SD_SPI", configMINIMAL_STACK_SIZE, NULL/*parametros*/, tskIDLE_PRIORITY+2, NULL /*handler*/);

	xTaskCreate(T_detectar_pulsador, "T_detectar_pulsador", configMINIMAL_STACK_SIZE, NULL/*parametros*/, tskIDLE_PRIORITY+1, NULL /*handler*/);

	xTaskCreate(T_LDR, "T_LDR", configMINIMAL_STACK_SIZE, NULL/*parametros*/, tskIDLE_PRIORITY+1, NULL /*handler*/);
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV8;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_12;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 100000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 49999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1000;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 700;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_SPI_CS_GPIO_Port, SD_SPI_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TFT_DC_Pin|TFT_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TFT_SPI_CS_GPIO_Port, TFT_SPI_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_SPI_CS_Pin */
  GPIO_InitStruct.Pin = SD_SPI_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SD_SPI_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TFT_DC_Pin TFT_RESET_Pin */
  GPIO_InitStruct.Pin = TFT_DC_Pin|TFT_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : TFT_SPI_CS_Pin */
  GPIO_InitStruct.Pin = TFT_SPI_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(TFT_SPI_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BOTON_DOWN_Pin EXTBUTTON_Pin BOTON_LEFT_Pin */
  GPIO_InitStruct.Pin = BOTON_DOWN_Pin|EXTBUTTON_Pin|BOTON_LEFT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BOTON_UP_Pin BOTON_RIGHT_Pin */
  GPIO_InitStruct.Pin = BOTON_UP_Pin|BOTON_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
